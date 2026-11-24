#pragma once

#include <cstdint>
#include <random>

// the draws the game makes from its Mersenne twister Random
namespace GameRandom {
    inline uint32_t nextUnsigned(std::mt19937 &random) {
        return random();
    }

    inline int32_t nextInt(std::mt19937 &random) {
        return static_cast<int32_t>(random() >> 1);
    }

    inline int32_t nextInt(std::mt19937 &random, int32_t bound) {
        return bound != 0 ? static_cast<int32_t>(random() % static_cast<uint32_t>(bound)) : 0;
    }

    // one raw draw scaled to [0, 1)
    inline float toFloat(uint32_t value) {
        return static_cast<float>(static_cast<double>(value) * 2.3283064365386963e-10);
    }

    inline float nextFloat(std::mt19937 &random) {
        return toFloat(random());
    }
}
