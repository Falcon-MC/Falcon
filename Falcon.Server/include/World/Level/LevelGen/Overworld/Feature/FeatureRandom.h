#pragma once

#include "World/Level/LevelGen/Random/GameRandom.h"

#include <cstdint>
#include <random>

// the draws the game's features make from its Mersenne twister Random
namespace FeatureRandom {
    using GameRandom::nextFloat;
    using GameRandom::nextInt;

    // one raw draw scaled to [0, 1)
    inline double nextDouble(std::mt19937 &random) {
        return static_cast<double>(random()) * 2.3283064365386963e-10;
    }

    // a boolean reads bit 27 of a raw draw
    inline bool nextBoolean(std::mt19937 &random) {
        return (random() & 0x8000000u) != 0;
    }

    // min + nextInt(max - min), min alone when the range is empty: the maximum is never drawn
    inline int32_t between(std::mt19937 &random, int32_t min, int32_t max) {
        return max > min ? min + nextInt(random, max - min) : min;
    }
}
