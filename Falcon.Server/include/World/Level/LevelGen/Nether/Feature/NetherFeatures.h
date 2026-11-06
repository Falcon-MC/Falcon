#pragma once

#include <cstdint>
#include <random>

class BlockManager;

// the game's nether features, placed by the biome rules at a scattered position with the rule's placement random
namespace NetherFeatures {
    // sub_14A2C77B0, built by sub_14A2C75F0
    bool placeHugeCrimsonFungus(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    bool placeHugeWarpedFungus(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    // sub_14721C330
    bool placeWeepingVines(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    // sub_1471EB280
    bool placeTwistingVines(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    // sub_147172D80
    bool placeBasaltPillar(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    // sub_14D206500
    bool placeDelta(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    // sub_1471725C0
    bool placeBasaltColumns(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);
}
