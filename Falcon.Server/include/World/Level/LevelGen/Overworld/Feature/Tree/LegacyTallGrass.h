#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

#include <cstdint>

class BlockManager;

class LegacyTallGrass {
public:
    static void growGrass(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random);
};
