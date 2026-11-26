#pragma once

#include <cstdint>
#include <random>

class BlockManager;

// minecraft:sculk_patch_feature of the deep dark: charges spread sculk and veins from the position, then a catalyst
class SculkPatchFeature {
public:
    static bool place(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);
};
