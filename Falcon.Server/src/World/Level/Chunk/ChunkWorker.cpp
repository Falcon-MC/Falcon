#include "World/Level/Chunk/ChunkWorker.h"

#include "World/Level/LevelGen/GeneratorChunkSource.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/Storage/LevelStorage.h"
#include "World/Level/LightSystem.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <pthread.h>
#ifdef __APPLE__
#include <pthread/qos.h>
#endif
#include <sched.h>

namespace {
    // lower quarter of the scheduling range: generation and storage never starve the tick thread
    void lowerCurrentThreadPriority() {
#ifdef __APPLE__
        // the scheduler picks cores by class there: below the tick, still on the fast cores; utility kept the
        // workers on the efficiency cores and halved generation
        if (pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0) == 0)
            return;
#endif
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
          mLoadedCount(0), mSavedCount(0), mPopulatedCount(0), mLastGeneration(std::chrono::steady_clock::now()) {}

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
    mTerrainCache.reset(new TerrainCache(mGenerator));
    for (size_t i = 0; i < count; ++i)
        mSources.push_back(std::unique_ptr<GeneratorChunkSource>(
                new GeneratorChunkSource(mGenerator.getSeed(), mGenerator.getDimensionType(), *mTerrainCache)));

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
    mTerrainCache.reset();
    mCompleted.close();
}

// one queue per chunk: a save and a later load run in order on one thread, no storage race; fully
// mixed: a player's streamed chunks spread over the workers
size_t ChunkWorker::_queueIndexFor(int32_t chunkX, int32_t chunkZ) const {
    uint64_t key = ((uint64_t) (uint32_t) chunkX << 32) | (uint32_t) chunkZ;
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdull;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ull;
    key ^= key >> 33;
    return (size_t) (key % mQueues.size());
}

bool ChunkWorker::_reserveGeneration() {
    if (!mRunning.load() || mDiscardGeneration.load())
        return false;

    size_t count = mOutstandingGeneration.load();
    while (count < MAX_OUTSTANDING_GENERATION) {
        if (mOutstandingGeneration.compare_exchange_weak(count, count + 1))
            return true;
    }
    return false;
}

bool ChunkWorker::requestLoad(int32_t chunkX, int32_t chunkZ, std::vector<GeneratedBlockChange> &parked) {
    if (mQueues.empty() || !_reserveGeneration())
        return false;

    ChunkTask task;
    task.mKind = ChunkTask::Kind::Load;
    task.mX = chunkX;
    task.mZ = chunkZ;
    task.mChanges = std::move(parked);
    parked.clear();
    mQueues[_queueIndexFor(chunkX, chunkZ)]->push(std::move(task));
    return true;
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

size_t ChunkWorker::getPendingSaveCount() const {
    std::lock_guard<std::mutex> lock(mPendingSavesMutex);
    return mPendingSaves.size();
}

bool ChunkWorker::hasPendingSave(int32_t chunkX, int32_t chunkZ) const {
    std::lock_guard<std::mutex> lock(mPendingSavesMutex);
    const int64_t key = _packChunk(chunkX, chunkZ);
    return mPendingSaves.count(key) != 0 || mFailedSaves.count(key) != 0;
}

bool ChunkWorker::hasFailedSave(int32_t chunkX, int32_t chunkZ) const {
    std::lock_guard<std::mutex> lock(mPendingSavesMutex);
    return mFailedSaves.count(_packChunk(chunkX, chunkZ)) != 0;
}

bool ChunkWorker::waitForSaves() {
    std::vector<std::future<void>> barriers;
    for (auto &queue: mQueues) {
        ChunkTask task;
        task.mKind = ChunkTask::Kind::Barrier;
        task.mBarrier = std::make_shared<std::promise<void>>();
        barriers.push_back(task.mBarrier->get_future());
        queue->push(std::move(task));
    }
    for (auto &barrier: barriers)
        barrier.get();
    std::lock_guard<std::mutex> lock(mPendingSavesMutex);
    return mFailedSaves.empty();
}

bool ChunkWorker::requestPopulate(std::unique_ptr<LevelChunk> chunk) {
    if (chunk == nullptr || mQueues.empty() || !_reserveGeneration())
        return false;

    ChunkTask task;
    task.mKind = ChunkTask::Kind::Populate;
    task.mX = chunk->getX();
    task.mZ = chunk->getZ();
    task.mChunk = std::move(chunk);
    mQueues[_queueIndexFor(task.mX, task.mZ)]->push(std::move(task));
    return true;
}

std::future<std::vector<GeneratedBlockChange>>
ChunkWorker::requestPersistChanges(int32_t chunkX, int32_t chunkZ, std::vector<GeneratedBlockChange> changes) {
    ChunkTask task;
    task.mKind = ChunkTask::Kind::PersistChanges;
    task.mX = chunkX;
    task.mZ = chunkZ;
    task.mChanges = std::move(changes);
    task.mChangesResult = std::make_shared<std::promise<std::vector<GeneratedBlockChange>>>();
    auto result = task.mChangesResult->get_future();
    if (mQueues.empty() || !mRunning.load()) {
        task.mChangesResult->set_value(std::move(task.mChanges));
        return result;
    }
    mQueues[_queueIndexFor(chunkX, chunkZ)]->push(std::move(task));
    return result;
}

std::future<void> ChunkWorker::requestEraseEntities(std::vector<int64_t> uniqueIds) {
    ChunkTask task;
    task.mKind = ChunkTask::Kind::EraseEntities;
    task.mEntityIds = std::move(uniqueIds);
    task.mBarrier = std::make_shared<std::promise<void>>();
    std::future<void> done = task.mBarrier->get_future();
    if (mQueues.empty() || !mRunning.load()) {
        for (const int64_t uniqueId: task.mEntityIds)
            mStorage.eraseEntity(uniqueId);
        task.mBarrier->set_value();
        return done;
    }
    mQueues[0]->push(std::move(task));
    return done;
}

std::vector<ChunkLoadResult> ChunkWorker::drainCompleted() {
    auto results = mCompleted.drain();
    mOutstandingGeneration.fetch_sub(results.size());

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (mOutstandingGeneration.load() != 0 || !results.empty()) {
        mLastGeneration = now;
        mCachesReleased = false;
    } else if (!mCachesReleased && mTerrainCache != nullptr && now - mLastGeneration >= CACHE_IDLE_RELEASE) {
        mTerrainCache->clear();
        mCachesReleased = true;
    }
    return results;
}

uint64_t ChunkWorker::getTerrainGenerations() const {
    return mTerrainCache == nullptr ? 0 : mTerrainCache->getTerrainGenerations();
}

uint64_t ChunkWorker::getDecorationCount() const {
    return mTerrainCache == nullptr ? 0 : mTerrainCache->getDecorationCount();
}

void ChunkWorker::_shareNeeds(int32_t chunkX, int32_t chunkZ, size_t sourceIndex) {
    if (mQueues.size() < 2 || sourceIndex >= mSources.size())
        return;

    // only to idle workers: busy ones build what their own chunks need, mostly the same
    std::vector<size_t> idle;
    for (size_t queue = 0; queue < mQueues.size(); queue++) {
        if (queue != sourceIndex && mQueues[queue]->empty())
            idle.push_back(queue);
    }
    if (idle.empty())
        return;

    const std::vector<GeneratorChunkSource::Need> needs = mSources[sourceIndex]->missing(chunkX, chunkZ);
    // reversed onto the fronts: each queue takes its share in order, terrain first
    size_t next = 0;
    for (auto need = needs.rbegin(); need != needs.rend(); ++need) {
        ChunkTask task;
        task.mKind = ChunkTask::Kind::Prepare;
        task.mX = need->mX;
        task.mZ = need->mZ;
        task.mPrepareDecoration = need->mDecoration;
        mQueues[idle[next++ % idle.size()]]->pushFront(std::move(task));
    }
    // this worker builds the terrain in the same order, skipping what the others build; its own chunk's
    // decorations then take the free ones first
    for (const GeneratorChunkSource::Need &need: needs) {
        if (need.mDecoration)
            break;
        mSources[sourceIndex]->prepare(need);
    }
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
        _finishChunk(std::move(chunk), sourceIndex, false, false, std::move(task.mChanges));
        return;
    }

    mGeneratedCount.fetch_add(1);
    _finishChunk(std::move(chunk), sourceIndex, false, true, std::move(task.mChanges));
}

void ChunkWorker::_finishChunk(std::unique_ptr<LevelChunk> chunk, size_t sourceIndex, bool replacesResident,
                               bool generated, std::vector<GeneratedBlockChange> parked) {
    ChunkLoadResult result;
    result.mX = chunk->getX();
    result.mZ = chunk->getZ();
    result.mReplacesResident = replacesResident;
    result.mGenerated = generated;

    // blocks spilled by neighbour features while absent; in-memory ones are newer than the stored ones: last
    std::vector<GeneratedBlockChange> spilled;
    if (mStorage.isOpen())
        spilled = mStorage.loadPendingBlockChanges(result.mX, result.mZ);
    const bool stored = !spilled.empty();
    spilled.insert(spilled.end(), std::make_move_iterator(parked.begin()), std::make_move_iterator(parked.end()));

    const auto applySpilled = [&]() {
        for (const GeneratedBlockChange &change: spilled) {
            if (change.mY < LevelChunk::MIN_Y || change.mY > LevelChunk::MAX_Y)
                continue;

            chunk->setBlock(change.mX & 15, change.mY, change.mZ & 15, change.mLayer, *change.mState);
        }
    };

    if (generated && sourceIndex >= mSources.size()) {
        *chunk = mTerrainCache->terrain(result.mX, result.mZ);
        applySpilled();
    } else if (!chunk->isPopulated() && sourceIndex < mSources.size()) {
        std::vector<Tag> blockActors;
        std::vector<Tag> entities;
        // an older world's neighbour parked its share of its features here: its decoration is not replayed again
        uint16_t skipped = 0;
        if (!spilled.empty() && mStorage.isOpen()) {
            for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
                for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
                    if ((offsetX != 0 || offsetZ != 0) && mStorage.hasChunk(result.mX + offsetX, result.mZ + offsetZ))
                        skipped |= (uint16_t) (1u << ((offsetX + 1) * 3 + offsetZ + 1));
                }
            }
        }
        if (generated) {
            _shareNeeds(result.mX, result.mZ, sourceIndex);
            *chunk = mSources[sourceIndex]->generate(result.mX, result.mZ, blockActors, entities, skipped);
        } else {
            if (!chunk->hasHeightmap())
                LightSystem::computeHeightmap(*chunk);
            mSources[sourceIndex]->populate(*chunk, blockActors, entities, skipped);
        }
        // spills of an older world, whose neighbours' decorations wrote into absent chunks
        applySpilled();
        chunk->markDirty();
        mPopulatedCount.fetch_add(1);

        if (!blockActors.empty() && mStorage.isOpen()) {
            std::vector<Tag> saved = mStorage.loadBlockEntities(result.mX, result.mZ);
            saved.insert(saved.end(), blockActors.begin(), blockActors.end());
            mStorage.saveBlockEntities(result.mX, result.mZ, saved);
        }
        result.mBlockActors = std::move(blockActors);
        result.mEntities = std::move(entities);
    } else {
        applySpilled();
    }

    if (stored)
        mStorage.erasePendingBlockChanges(result.mX, result.mZ);
    // applied by the worker; exist nowhere else: restored if the result is dropped
    result.mSpilledChanges = std::move(spilled);

    chunk->compactBiomes();
    // no sky light: only read around players, computed by Level on first use; most streamed chunks never
    // need its arrays
    if (Dimension::hasSkyLight(chunk->getDimension()))
        LightSystem::computeHeightmap(*chunk);
    LightSystem::computeBlockLight(*chunk);

    result.mChunk = std::move(chunk);

    mCompleted.push(std::move(result));
}

void ChunkWorker::_processSave(ChunkTask &task) {
    bool saved = false;
    try {
        saved = task.mChunk != nullptr && mStorage.isOpen() && mStorage.saveChunk(*task.mChunk);
    } catch (...) {
        saved = false;
    }
    if (saved)
        mSavedCount.fetch_add(1);

    std::lock_guard<std::mutex> lock(mPendingSavesMutex);
    const int64_t key = _packChunk(task.mX, task.mZ);
    if (saved)
        mFailedSaves.erase(key);
    else
        mFailedSaves.insert(key);
    const auto pending = mPendingSaves.find(key);
    if (pending != mPendingSaves.end() && --pending->second == 0)
        mPendingSaves.erase(pending);
}

void ChunkWorker::_processPersistChanges(ChunkTask &task) {
    // several chunks per task, merged one by one; only the failed ones are handed back
    std::unordered_map<int64_t, std::vector<GeneratedBlockChange>> byChunk;
    for (GeneratedBlockChange &change: task.mChanges) {
        const int64_t key = ((int64_t) (change.mX >> 4) << 32) | (uint32_t) (change.mZ >> 4);
        byChunk[key].push_back(std::move(change));
    }

    std::vector<GeneratedBlockChange> failed;
    for (auto &entry: byChunk) {
        bool saved = false;
        try {
            saved = mStorage.mergePendingBlockChanges((int32_t) (entry.first >> 32),
                                                      (int32_t) (entry.first & 0xffffffff), entry.second);
        } catch (...) {
            saved = false;
        }
        if (!saved)
            failed.insert(failed.end(), std::make_move_iterator(entry.second.begin()),
                          std::make_move_iterator(entry.second.end()));
    }

    task.mChangesResult->set_value(std::move(failed));
    task.mChangesResult.reset();
}

void ChunkWorker::_run(size_t queueIndex) {
    lowerCurrentThreadPriority();

    TaskQueue<ChunkTask> &queue = *mQueues[queueIndex];
    ChunkTask task;

    while (queue.waitPop(task)) {
        if (task.mKind == ChunkTask::Kind::Barrier) {
            task.mBarrier->set_value();
            task.mBarrier.reset();
            continue;
        }
        if (task.mKind == ChunkTask::Kind::PersistChanges) {
            _processPersistChanges(task);
            continue;
        }
        // no credit: dropped when generation is discarded
        if (task.mKind == ChunkTask::Kind::Prepare) {
            if (!mDiscardGeneration.load()
                && !mSources[queueIndex]->prepare({task.mX, task.mZ, task.mPrepareDecoration})) {
                // behind the rest of the queue; alone, after the next decoration ends instead of spinning
                if (queue.empty())
                    mTerrainCache->awaitDecoration(PREPARE_RETRY);
                queue.push(std::move(task));
                task = ChunkTask();
            }
            continue;
        }
        if (task.mKind == ChunkTask::Kind::EraseEntities) {
            for (const int64_t uniqueId: task.mEntityIds)
                mStorage.eraseEntity(uniqueId);
            task.mEntityIds.clear();
            task.mBarrier->set_value();
            task.mBarrier.reset();
            continue;
        }
        // saves never dropped: player changes would be lost
        if (task.mKind != ChunkTask::Kind::Save && mDiscardGeneration.load()) {
            // parked blocks of a dropped load exist nowhere else: back to the main thread for the final save;
            // the result returns the generation credit
            if (task.mKind == ChunkTask::Kind::Load && !task.mChanges.empty()) {
                ChunkLoadResult result;
                result.mX = task.mX;
                result.mZ = task.mZ;
                result.mSpilledChanges = std::move(task.mChanges);
                mCompleted.push(std::move(result));
            } else {
                mOutstandingGeneration.fetch_sub(1);
            }
            task.mChunk.reset();
            task.mChanges.clear();
            continue;
        }

        if (task.mKind == ChunkTask::Kind::Load)
            _processLoad(task, queueIndex);
        else if (task.mKind == ChunkTask::Kind::Populate)
            _finishChunk(std::move(task.mChunk), queueIndex, true, false, std::move(task.mChanges));
        else if (task.mKind == ChunkTask::Kind::Save)
            _processSave(task);

        task.mChunk.reset();
        task.mChanges.clear();
    }
}
