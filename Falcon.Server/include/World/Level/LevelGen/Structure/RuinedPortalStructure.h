#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class ChunkGenerator;
class Level;
class LevelChunk;
class TerrainCache;

// the whole portal is worked out by the start chunk; each chunk
// it reaches places its own share of the result
class RuinedPortalStructure {
public:
    struct Change {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        // 1: the water of a waterlogged block
        int32_t mLayer = 0;
        BlockState mBlock;
    };

    struct Chest {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        std::string mLootTable;
        int32_t mSeed = 0;
    };

    struct Plan {
        std::string mTemplate;
        int32_t mQuarterTurns = 0;
        int32_t mMirror = 0;
        int32_t mMinX = 0;
        int32_t mMinY = 0;
        int32_t mMinZ = 0;
        int32_t mMaxX = 0;
        int32_t mMaxY = 0;
        int32_t mMaxZ = 0;
        std::vector<Change> mChanges;
        std::vector<Chest> mChests;
    };

    // read on the bare terrain: the game reads the neighbour chunks in whatever state they are
    static std::shared_ptr<const Plan> plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ,
                                            TerrainCache *cache = nullptr);

    static void place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk);
};
