#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <map>
#include <memory>
#include <utility>

class ChunkGenerator;
class LevelChunk;

/**
 * The bare terrain under a structure being planned. A structure reads many columns of few chunks, and a throwaway
 * chunk is the expensive part.
 */
class StructureTerrain {
public:
    explicit StructureTerrain(const ChunkGenerator &generator);
    ~StructureTerrain();

    int32_t getSolidHeight(int32_t x, int32_t z);

    // one above the highest block that is not air, water included
    int32_t getTopHeight(int32_t x, int32_t z);

    const BlockState &getBlock(int32_t x, int32_t y, int32_t z);

private:
    const LevelChunk *_chunkAt(int32_t x, int32_t z);

    const ChunkGenerator &mGenerator;
    std::map<std::pair<int32_t, int32_t>, std::unique_ptr<LevelChunk>> mChunks;
};
