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

void ChunkWorker::requestSave(std::unique_ptr<LevelChunk> chunk) {
    if (chunk == nullptr || mQueues.empty())
        return;

    ChunkTask task;
    task.mKind = ChunkTask::Kind::Save;
    task.mX = chunk->getX();
    task.mZ = chunk->getZ();
    task.mChunk = std::move(chunk);
    mQueues[_queueIndexFor(task.mX, task.mZ)]->push(std::move(task));
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

    if (mStorage.isOpen() && mStorage.loadChunk(*chunk)) {
        mLoadedCount.fetch_add(1);
    } else {
        mGenerator.generate(*chunk);
        mGeneratedCount.fetch_add(1);
    }

    _finishChunk(std::move(chunk), sourceIndex, false);
}

void ChunkWorker::_finishChunk(std::unique_ptr<LevelChunk> chunk, size_t sourceIndex, bool replacesResident) {
    ChunkLoadResult result;
    result.mX = chunk->getX();
    result.mZ = chunk->getZ();
    result.mReplacesResident = replacesResident;

    if (!chunk->isPopulated() && sourceIndex < mSources.size()) {
        if (!chunk->hasHeightmap())
            LightSystem::computeHeightmap(*chunk);

        mSources[sourceIndex]->populate(*chunk, result.mOverflowChanges);
        chunk->setPopulated(true);
        chunk->markDirty();
        mPopulatedCount.fetch_add(1);
    }

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

    chunk->buildNetworkCaches();

    result.mNetworkSubChunkCount = chunk->getNetworkSubChunkCount();
    result.mNetworkData = chunk->encodeNetwork();

    result.mChunk = std::move(chunk);

    mCompleted.push(std::move(result));
}

void ChunkWorker::_processSave(ChunkTask &task) {
    if (task.mChunk == nullptr || !mStorage.isOpen())
        return;

    if (mStorage.saveChunk(*task.mChunk))
        mSavedCount.fetch_add(1);
}

void ChunkWorker::_run(size_t queueIndex) {
    lowerCurrentThreadPriority();

    TaskQueue<ChunkTask> &queue = *mQueues[queueIndex];
    ChunkTask task;

    while (queue.waitPop(task)) {
        if (task.mKind != ChunkTask::Kind::Save && mDiscardGeneration.load()) {
            task.mChunk.reset();
            continue;
        }

        if (task.mKind == ChunkTask::Kind::Load)
            _processLoad(task, queueIndex);
        else if (task.mKind == ChunkTask::Kind::Populate)
            _finishChunk(std::move(task.mChunk), queueIndex, true);
        else
            _processSave(task);

        task.mChunk.reset();
    }
}
