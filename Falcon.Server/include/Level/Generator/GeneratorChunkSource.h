#pragma once

#include "Level/GeneratedBlockChange.h"
#include "Level/Level.h"

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

class LevelChunk;

class GeneratorChunkSource {
public:
    static constexpr size_t MAX_CACHED_CHUNKS = 48;

    GeneratorChunkSource(int64_t seed, DimensionType dimension);

    GeneratorChunkSource(const GeneratorChunkSource &) = delete;

    GeneratorChunkSource &operator=(const GeneratorChunkSource &) = delete;

    void populate(LevelChunk &chunk, std::vector<GeneratedBlockChange> &overflow, std::vector<Tag> &blockActors);

    uint64_t getCacheHitCount() const { return mCacheHits; }

    uint64_t getCacheMissCount() const { return mCacheMisses; }

private:
    static int64_t _packChunk(int32_t chunkX, int32_t chunkZ);

    void _stageNeighbour(int32_t chunkX, int32_t chunkZ);

    void _retireNeighbour(int32_t chunkX, int32_t chunkZ);

    void _trim();

    Level mScratch;
    std::unordered_map<int64_t, LevelChunk> mTerrainCache;
    std::deque<int64_t> mCacheOrder;
    uint64_t mCacheHits = 0;
    uint64_t mCacheMisses = 0;
};
