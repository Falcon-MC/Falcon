#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

#include <cstdint>

class BlockManager;

class NyliumVegetationGenerator {
public:
    static void growVegetation(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random);
};
