#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <string>
#include <vector>

class ChunkGenerator;
class Level;
class LevelChunk;

// the whole wreck is decided in its start chunk, then every chunk it reaches writes its share
class ShipwreckStructure {
public:
    struct PlacedBlock {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        BlockState mState;
        std::string mLootTable;
        int32_t mLootSeed = 0;
        bool mChest = false;
    };

    // the blocks of the wreck of a start chunk in world coordinates, later entries replacing earlier ones
    static std::vector<PlacedBlock> build(const ChunkGenerator &generator, int32_t startX, int32_t startZ);

    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
};
