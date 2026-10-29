#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

#include <array>
#include <cstdint>

class Noise {
public:
    explicit Noise(IRandom &rand);

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
