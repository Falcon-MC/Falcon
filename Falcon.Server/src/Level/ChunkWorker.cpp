#include "Level/ChunkWorker.h"

#include "Level/Generator/GeneratorChunkSource.h"
#include "Level/Generator/ChunkGenerator.h"
#include "Level/LevelStorage.h"
#include "Level/LightSystem.h"

#include <algorithm>
#include <utility>

#include <pthread.h>
#include <sched.h>

namespace {
    /**
     * Drops the calling worker to the lower quarter of its scheduling range so chunk
     * generation and storage never starve the main tick thread.
     */
    void lowerCurrentThreadPriority() {
        sched_param parameters;
        int policy = 0;

        if (pthread_getschedparam(pthread_self(), &policy, &parameters) != 0)
            return;

        const int minimum = sched_get_priority_min(policy);
        const int maximum = sched_get_priority_max(policy);
        if (minimum < 0 || maximum < 0 || minimum >= maximum)
            return;

        parameters.sched_priority = minimum + (maximum - minimum) / 4;
        pthread_setschedparam(pthread_self(), policy, &parameters);
    }
}

ChunkWorker::ChunkWorker(const ChunkGenerator &generator, LevelStorage &storage)
        : mGenerator(generator), mStorage(storage), mRunning(false), mDiscardGeneration(false), mGeneratedCount(0),
          mLoadedCount(0), mSavedCount(0), mPopulatedCount(0) {}

ChunkWorker::~ChunkWorker() {
    stop();
}

void ChunkWorker::start(size_t threadCount) {
    if (mRunning.load())
        return;

    const size_t count = std::max<size_t>(1, std::min(threadCount, MAX_THREADS));

    mQueues.clear();
    mQueues.reserve(count);
    for (size_t i = 0; i < count; ++i)
        mQueues.push_back(std::unique_ptr<TaskQueue<ChunkTask>>(new TaskQueue<ChunkTask>()));

    mSources.clear();
    mSources.reserve(count);
    for (size_t i = 0; i < count; ++i)
        mSources.push_back(std::unique_ptr<GeneratorChunkSource>(
                new GeneratorChunkSource(mGenerator.getSeed(), mGenerator.getDimensionType())));

    mRunning.store(true);
    mDiscardGeneration.store(false);

    mThreads.reserve(count);
    for (size_t i = 0; i < count; ++i)
        mThreads.push_back(std::thread(&ChunkWorker::_run, this, i));
}

void ChunkWorker::discardPendingGeneration() {
    mDiscardGeneration.store(true);
}

void ChunkWorker::stop() {
    if (!mRunning.load())
        return;

    mRunning.store(false);

    for (std::unique_ptr<TaskQueue<ChunkTask>> &queue: mQueues)
        queue->close();

    for (std::thread &thread: mThreads) {
        if (thread.joinable())
            thread.join();
    }

    mThreads.clear();
    mQueues.clear();
    mSources.clear();
    mCompleted.close();
}

/**
 * Every task for a given chunk lands on the same queue, so a save and a later load of that
 * chunk are processed in order by one thread and never race in storage.
 */
size_t ChunkWorker::_queueIndexFor(int32_t chunkX, int32_t chunkZ) const {
    const uint64_t key = ((uint64_t) (uint32_t) (chunkX >> 1) << 32) | (uint32_t) (chunkZ >> 1);
    return (size_t) ((key * 1099511628211ull) >> 32) % mQueues.size();
}

void ChunkWorker::requestLoad(int32_t chunkX, int32_t chunkZ) {
    if (mQueues.empty())
        return;

    ChunkTask task;
    task.mKind = ChunkTask::Kind::Load;
    task.mX = chunkX;
    task.mZ = chunkZ;
    mQueues[_queueIndexFor(chunkX, chunkZ)]->push(std::move(task));
}

int64_t ChunkWorker::_packChunk(int32_t chunkX, int32_t chunkZ) {
    return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
}

void ChunkWorker::requestSave(std::unique_ptr<LevelChunk> chunk) {
    if (chunk == nullptr || mQueues.empty())
        return;

    ChunkTask task;
    task.mKind = ChunkTask::Kind::Save;
    task.mX = chunk->getX();
    task.mZ = chunk->getZ();
    task.mChunk = std::move(chunk);

    {
        std::lock_guard<std::mutex> lock(mPendingSavesMutex);
        mPendingSaves[_packChunk(task.mX, task.mZ)]++;
    }

    mQueues[_queueIndexFor(task.mX, task.mZ)]->push(std::move(task));
}

bool ChunkWorker::hasPendingSave(int32_t chunkX, int32_t chunkZ) const {
    std::lock_guard<std::mutex> lock(mPendingSavesMutex);
    return mPendingSaves.find(_packChunk(chunkX, chunkZ)) != mPendingSaves.end();
}

void ChunkWorker::requestPopulate(std::unique_ptr<LevelChunk> chunk) {
    if (chunk == nullptr || mQueues.empty())
        return;

    ChunkTask task;
    task.mKind = ChunkTask::Kind::Populate;
    task.mX = chunk->getX();
    task.mZ = chunk->getZ();
    task.mChunk = std::move(chunk);
    mQueues[_queueIndexFor(task.mX, task.mZ)]->push(std::move(task));
}

std::vector<ChunkLoadResult> ChunkWorker::drainCompleted() {
    return mCompleted.drain();
}

size_t ChunkWorker::getPendingTaskCount() const {
    size_t total = 0;
    for (const std::unique_ptr<TaskQueue<ChunkTask>> &queue: mQueues)
        total += queue->size();

    return total;
}

void ChunkWorker::_processLoad(ChunkTask &task, size_t sourceIndex) {
    std::unique_ptr<LevelChunk> chunk(new LevelChunk(task.mX, task.mZ));
    chunk->setDimension(mGenerator.getDimensionType());

    bool generated = false;
    if (mStorage.isOpen() && mStorage.loadChunk(*chunk)) {
        mLoadedCount.fetch_add(1);
    } else {
        mGenerator.generate(*chunk);
        mGeneratedCount.fetch_add(1);
        generated = true;
    }

    _finishChunk(std::move(chunk), sourceIndex, false, generated);
}

void ChunkWorker::_finishChunk(std::unique_ptr<LevelChunk> chunk, size_t sourceIndex, bool replacesResident,
                               bool generated) {
    ChunkLoadResult result;
    result.mX = chunk->getX();
    result.mZ = chunk->getZ();
    result.mReplacesResident = replacesResident;
    result.mGenerated = generated;

    if (!chunk->isPopulated() && sourceIndex < mSources.size()) {
        if (!chunk->hasHeightmap())
            LightSystem::computeHeightmap(*chunk);

        std::vector<Tag> blockActors;
        mSources[sourceIndex]->populate(*chunk, result.mOverflowChanges, blockActors);
        chunk->setPopulated(true);
        chunk->markDirty();
        mPopulatedCount.fetch_add(1);

        if (!blockActors.empty() && mStorage.isOpen()) {
            std::vector<Tag> stored = mStorage.loadBlockEntities(result.mX, result.mZ);
            stored.insert(stored.end(), blockActors.begin(), blockActors.end());
            mStorage.saveBlockEntities(result.mX, result.mZ, stored);
        }
    }

    // Features populated in a neighbouring chunk may have spilled blocks into this one while
    // it was not loaded; those were stored and are applied now.
    if (mStorage.isOpen()) {
        const std::vector<GeneratedBlockChange> stored = mStorage.loadPendingBlockChanges(result.mX, result.mZ);

        if (!stored.empty()) {
            for (const GeneratedBlockChange &change: stored) {
                if (change.mY < LevelChunk::MIN_Y || change.mY > LevelChunk::MAX_Y)
                    continue;

                chunk->setBlock(change.mX & 15, change.mY, change.mZ & 15, change.mState);
            }

            mStorage.erasePendingBlockChanges(result.mX, result.mZ);
        }
    }

    LightSystem::computeSkyLight(*chunk);
    LightSystem::computeBlockLight(*chunk);

    // Lighting and network encoding are done here, off the main thread, so the main thread
    // only has to swap the finished chunk in.
    chunk->buildNetworkCaches();

    result.mChunk = std::move(chunk);

    mCompleted.push(std::move(result));
}

void ChunkWorker::_processSave(ChunkTask &task) {
    if (task.mChunk != nullptr && mStorage.isOpen() && mStorage.saveChunk(*task.mChunk))
        mSavedCount.fetch_add(1);

    std::lock_guard<std::mutex> lock(mPendingSavesMutex);
    const auto pending = mPendingSaves.find(_packChunk(task.mX, task.mZ));
    if (pending != mPendingSaves.end() && --pending->second == 0)
        mPendingSaves.erase(pending);
}

void ChunkWorker::_run(size_t queueIndex) {
    lowerCurrentThreadPriority();

    TaskQueue<ChunkTask> &queue = *mQueues[queueIndex];
    ChunkTask task;

    while (queue.waitPop(task)) {
        // Saves are never dropped, even when pending generation is discarded, or player
        // changes would be lost.
        if (task.mKind != ChunkTask::Kind::Save && mDiscardGeneration.load()) {
            task.mChunk.reset();
            continue;
        }

        if (task.mKind == ChunkTask::Kind::Load)
            _processLoad(task, queueIndex);
        else if (task.mKind == ChunkTask::Kind::Populate)
            _finishChunk(std::move(task.mChunk), queueIndex, true, false);
        else
            _processSave(task);

        task.mChunk.reset();
    }
}
