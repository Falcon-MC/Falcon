#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <map>
#include <memory>
#include <utility>

class ChunkGenerator;
class LevelChunk;
class TerrainCache;

// bare terrain under a planned structure: many columns of few chunks, the throwaway chunk being the costly part;
// read from the workers' cache when there is one
class StructureTerrain {
public:
    explicit StructureTerrain(const ChunkGenerator &generator, TerrainCache *cache = nullptr);
    ~StructureTerrain();

    int32_t getSolidHeight(int32_t x, int32_t z);

    // one above the highest block the chunk heightmap counts, water included
    int32_t getTopHeight(int32_t x, int32_t z);

    const BlockState &getBlock(int32_t x, int32_t y, int32_t z);

private:
    const LevelChunk *_chunkAt(int32_t x, int32_t z);

    const ChunkGenerator &mGenerator;
    TerrainCache *mCache;
    std::map<std::pair<int32_t, int32_t>, std::shared_ptr<const LevelChunk>> mChunks;
};
