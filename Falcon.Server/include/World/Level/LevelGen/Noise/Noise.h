#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

#include <array>
#include <cstdint>
#include <random>

class Noise {
public:
    explicit Noise(IRandom &rand);

    // drawn from the game's Mersenne twister Random: nextDouble, then nextInt(256 - i) (sub_145C2AF90)
    explicit Noise(std::mt19937 &random);

    virtual ~Noise() = default;

    double getOriginX() const;

    double getOriginY() const;

    double getOriginZ() const;

protected:
    int32_t lookup(int32_t hash) const;

    double mOriginX;
    double mOriginY;
    double mOriginZ;
    std::array<int8_t, 256> mPermutations;
};
