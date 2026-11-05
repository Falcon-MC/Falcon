#pragma once

#include "World/Level/LevelGen/Random/GameRandom.h"

#include <cstdint>
#include <random>

// the draws the game's features make from its Mersenne twister Random (vtable 0x14F104980)
namespace FeatureRandom {
    using GameRandom::nextFloat;
    using GameRandom::nextInt;

    // min + nextInt(max - min), min alone when the range is empty: the maximum is never drawn
    inline int32_t between(std::mt19937 &random, int32_t min, int32_t max) {
        return max > min ? min + nextInt(random, max - min) : min;
    }
}
