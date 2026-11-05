#pragma once

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

// the draws the game's features make from its Mersenne twister Random (vtable 0x14F104980)
namespace FeatureRandom {
    // Random::nextInt(n) (sub_141A7E190)
    inline int32_t nextInt(std::mt19937 &random, int32_t bound) {
        return bound != 0 ? static_cast<int32_t>(random() % static_cast<uint32_t>(bound)) : 0;
    }

    // Random::nextFloat (sub_141A7E230)
    inline float nextFloat(std::mt19937 &random) {
        return static_cast<float>(static_cast<double>(random()) * 2.3283064365386963e-10);
    }

    // min + nextInt(max - min), min alone when the range is empty: the maximum is never drawn
    inline int32_t between(std::mt19937 &random, int32_t min, int32_t max) {
        return max > min ? min + nextInt(random, max - min) : min;
    }

    // the game's sine table, filled with sinf (sub_14199D950)
    inline float sineAt(float scaled) {
        static const std::vector<float> table = [] {
            std::vector<float> values(65536);
            for (int32_t i = 0; i < 65536; i++)
                values[(size_t) i] = std::sin(static_cast<float>(i) / 10430.378f);
            return values;
        }();
        return table[static_cast<uint16_t>(static_cast<int32_t>(scaled))];
    }
}
