#pragma once

#include <cstdint>
#include <random>

class BlockManager;

// the game's nether features, placed by the biome rules at a scattered position with the rule's placement random
namespace NetherFeatures {
    // built by
    bool placeHugeCrimsonFungus(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    bool placeHugeWarpedFungus(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    bool placeWeepingVines(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    bool placeTwistingVines(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    bool placeBasaltPillar(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    bool placeDelta(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    bool placeBasaltColumns(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);
}
