#pragma once

#include "Core/Concurrency/TaskQueue.h"
#include "Level/GeneratedBlockChange.h"
#include "Level/LevelChunk.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class ChunkGenerator;
class LevelStorage;
class GeneratorChunkSource;

struct ChunkTask {
    enum class Kind : int {
        Load = 0,
        Save = 1,
        Populate = 2
    };

    Kind mKind = Kind::Load;
    int32_t mX = 0;
    int32_t mZ = 0;
    std::unique_ptr<LevelChunk> mChunk;
};

struct ChunkLoadResult {
    int32_t mX = 0;
    int32_t mZ = 0;
    std::unique_ptr<LevelChunk> mChunk;
    std::string mNetworkData;
    int mNetworkSubChunkCount = 0;
    std::vector<GeneratedBlockChange> mOverflowChanges;
    bool mReplacesResident = false;
};

class ChunkWorker {
public:
    static constexpr size_t MAX_THREADS = 64;

    ChunkWorker(const ChunkGenerator &generator, LevelStorage &storage);

    ~ChunkWorker();

    ChunkWorker(const ChunkWorker &) = delete;

    ChunkWorker &operator=(const ChunkWorker &) = delete;

    void start(size_t threadCount);

    void stop();

    bool isRunning() const { return mRunning.load(); }

    size_t getThreadCount() const { return mQueues.size(); }

    void requestLoad(int32_t chunkX, int32_t chunkZ);

    void requestSave(std::unique_ptr<LevelChunk> chunk);

    void requestPopulate(std::unique_ptr<LevelChunk> chunk);

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

    void _finishChunk(std::unique_ptr<LevelChunk> chunk, size_t sourceIndex, bool replacesResident);

    size_t _queueIndexFor(int32_t chunkX, int32_t chunkZ) const;

    const ChunkGenerator &mGenerator;
    LevelStorage &mStorage;

    std::vector<std::unique_ptr<GeneratorChunkSource>> mSources;
    std::vector<std::unique_ptr<TaskQueue<ChunkTask>>> mQueues;
    TaskQueue<ChunkLoadResult> mCompleted;

    std::vector<std::thread> mThreads;
    std::atomic<bool> mRunning;
    std::atomic<bool> mDiscardGeneration;
    std::atomic<uint64_t> mGeneratedCount;
    std::atomic<uint64_t> mLoadedCount;
    std::atomic<uint64_t> mSavedCount;
    std::atomic<uint64_t> mPopulatedCount;
};
