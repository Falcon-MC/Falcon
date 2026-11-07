#pragma once

#include "World/Level/Chunk/GeneratedBlockChange.h"
#include "World/Level/Level.h"

#include <cstdint>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class LevelChunk;

/**
 * Raw terrain shared by every worker of a level. Populating a chunk needs the terrain of its
 * eight neighbours, and adjacent chunks usually land on different workers, so a per-worker cache
 * would regenerate most of them. Entries are handed out as copies, so callers need no locking.
 */
class TerrainCache {
public:
    static constexpr size_t MAX_CACHED_CHUNKS = 1024;

    /**
     * Copies the cached terrain of a chunk into `out` and returns true. When it is not cached, the
     * caller is made responsible for generating it and must hand it over with publish(); a caller
     * asking for terrain another worker is generating waits for it instead of generating it again.
     */
    bool acquire(int32_t chunkX, int32_t chunkZ, LevelChunk &out);

    void publish(LevelChunk chunk);

    void put(LevelChunk chunk);

private:
    static int64_t _packChunk(int32_t chunkX, int32_t chunkZ);

    void _store(int64_t key, std::shared_ptr<const LevelChunk> chunk);

    std::mutex mMutex;
    std::condition_variable mPublished;
    std::unordered_set<int64_t> mGenerating;
    // shared and never changed once stored, so a worker copies one out after letting go of the lock
    std::unordered_map<int64_t, std::pair<std::shared_ptr<const LevelChunk>, std::list<int64_t>::iterator>> mChunks;
    std::list<int64_t> mOrder;
};

/**
 * Per-worker population context: a private scratch level that holds a chunk and its
 * neighbours while it is decorated. The neighbours come from the level's shared terrain cache.
 */
class GeneratorChunkSource {
public:
    GeneratorChunkSource(int64_t seed, DimensionType dimension, TerrainCache &cache);

    GeneratorChunkSource(const GeneratorChunkSource &) = delete;

    GeneratorChunkSource &operator=(const GeneratorChunkSource &) = delete;

    void populate(LevelChunk &chunk, std::vector<GeneratedBlockChange> &overflow, std::vector<Tag> &blockActors,
                  std::vector<Tag> &entities);

private:
    void _stageNeighbour(int32_t chunkX, int32_t chunkZ);

    Level mScratch;
    TerrainCache &mCache;
};
