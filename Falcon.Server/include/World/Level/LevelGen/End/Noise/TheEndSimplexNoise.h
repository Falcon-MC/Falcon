#pragma once

#include <array>
#include <cstdint>
#include <random>

// float 2D simplex noise; its origin is drawn but never read
class TheEndSimplexNoise {
public:
    explicit TheEndSimplexNoise(std::mt19937 &random);

    float getValue(float x, float z) const;

private:
    std::array<int32_t, 512> mPermutations;
};
