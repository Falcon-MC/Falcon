#pragma once

#include "Core/Threading/TaskQueue.h"
#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ChunkGenerator;
class LevelStorage;
class GeneratorChunkSource;
class TerrainCache;

struct ChunkTask {
    enum class Kind : int {
        Load = 0,
        Save = 1,
        Populate = 2,
        Barrier = 3,
        PersistChanges = 4,
        EraseEntities = 5
    };

    Kind mKind = Kind::Load;
    int32_t mX = 0;
    int32_t mZ = 0;
    std::unique_ptr<LevelChunk> mChunk;
    std::shared_ptr<std::promise<void>> mBarrier;
    std::vector<GeneratedBlockChange> mChanges;
    std::shared_ptr<std::promise<std::vector<GeneratedBlockChange>>> mChangesResult;
    std::vector<int64_t> mEntityIds;
};

struct ChunkLoadResult {
    int32_t mX = 0;
    int32_t mZ = 0;
    std::unique_ptr<LevelChunk> mChunk;
    std::vector<GeneratedBlockChange> mOverflowChanges;
    // applied by the worker, stored ones first; exist nowhere else: restored if the result is dropped;
    // a load discarded at shutdown returns no chunk, only its task's parked blocks
    std::vector<GeneratedBlockChange> mSpilledChanges;
    bool mReplacesResident = false;
    bool mGenerated = false;
};

// workers never touch resident chunks, only copies handed by value; results via drainCompleted()
class ChunkWorker {
public:
    static constexpr size_t MAX_THREADS = 64;
    // credits held until the main thread drains the result: no unbounded result queue; generation takes
    // hundreds of ms: several chunks in flight per worker
    static constexpr size_t MAX_OUTSTANDING_GENERATION = 64;

    ChunkWorker(const ChunkGenerator &generator, LevelStorage &storage);

    ~ChunkWorker();

    ChunkWorker(const ChunkWorker &) = delete;

    ChunkWorker &operator=(const ChunkWorker &) = delete;

    void start(size_t threadCount);

    void stop();

    bool isRunning() const { return mRunning.load(); }

    size_t getThreadCount() const { return mQueues.size(); }

    // `parked`: feature blocks spilled in while absent, applied before lighting
    bool requestLoad(int32_t chunkX, int32_t chunkZ, std::vector<GeneratedBlockChange> &parked);

    void requestSave(std::unique_ptr<LevelChunk> chunk);

    bool requestPopulate(std::unique_ptr<LevelChunk> chunk);

    std::future<std::vector<GeneratedBlockChange>> requestPersistChanges(
            int32_t chunkX, int32_t chunkZ, std::vector<GeneratedBlockChange> changes);

    // off the main thread; the future completes once every deletion ran
    std::future<void> requestEraseEntities(std::vector<int64_t> uniqueIds);

    size_t getOutstandingGenerationCount() const {
        return mOutstandingGeneration.load();
    }

    // meanwhile no free + synchronous write: the older queued copy would overwrite it
    bool hasPendingSave(int32_t chunkX, int32_t chunkZ) const;

    size_t getPendingSaveCount() const;

    bool hasFailedSave(int32_t chunkX, int32_t chunkZ) const;

    bool waitForSaves();

    void discardPendingGeneration();

    std::vector<ChunkLoadResult> drainCompleted();

    size_t getPendingTaskCount() const;

    uint64_t getGeneratedCount() const { return mGeneratedCount.load(); }

    uint64_t getLoadedCount() const { return mLoadedCount.load(); }

    uint64_t getSavedCount() const { return mSavedCount.load(); }

    uint64_t getPopulatedCount() const { return mPopulatedCount.load(); }

private:
    void _run(size_t queueIndex);

    void _processLoad(ChunkTask &task, size_t sourceIndex);

    void _processSave(ChunkTask &task);

    void _processPersistChanges(ChunkTask &task);

    bool _reserveGeneration();

    void _finishChunk(std::unique_ptr<LevelChunk> chunk, size_t sourceIndex, bool replacesResident,
                      bool generated, std::vector<GeneratedBlockChange> parked);

    size_t _queueIndexFor(int32_t chunkX, int32_t chunkZ) const;

    static int64_t _packChunk(int32_t chunkX, int32_t chunkZ);

    const ChunkGenerator &mGenerator;
    LevelStorage &mStorage;

    std::unique_ptr<TerrainCache> mTerrainCache;
    std::vector<std::unique_ptr<GeneratorChunkSource>> mSources;
    std::vector<std::unique_ptr<TaskQueue<ChunkTask>>> mQueues;
    TaskQueue<ChunkLoadResult> mCompleted;

    mutable std::mutex mPendingSavesMutex;
    std::unordered_map<int64_t, size_t> mPendingSaves;
    std::unordered_set<int64_t> mFailedSaves;

    std::vector<std::thread> mThreads;
    std::atomic<bool> mRunning;
    std::atomic<bool> mDiscardGeneration;
    std::atomic<size_t> mOutstandingGeneration{0};
    std::atomic<uint64_t> mGeneratedCount;
    std::atomic<uint64_t> mLoadedCount;
    std::atomic<uint64_t> mSavedCount;
    std::atomic<uint64_t> mPopulatedCount;
};
