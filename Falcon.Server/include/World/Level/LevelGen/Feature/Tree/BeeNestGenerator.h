#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

#include <cstdint>

class BlockManager;

class BeeNestGenerator {
public:
    static bool place(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z);

    static bool place(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t beeCount);

    static void placeAt(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t beeCount);

    static bool hasNearbyFlower(BlockManager &manager, int32_t x, int32_t y, int32_t z);
};
