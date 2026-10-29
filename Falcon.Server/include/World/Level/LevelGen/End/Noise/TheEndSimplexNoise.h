#pragma once

#include "World/Level/LevelGen/Random/IRandom.h"

#include <array>
#include <cstdint>

class TheEndSimplexNoise {
public:
    explicit TheEndSimplexNoise(IRandom &random);

    double getValue(double x, double z) const;

    double getOriginX() const { return mOriginX; }

    double getOriginY() const { return mOriginY; }

    double getOriginZ() const { return mOriginZ; }

private:
    static int32_t _fastFloor(double value);

    static double _dot(const int32_t *gradient, double x, double z);

    std::array<int32_t, 512> mPermutations;
    double mOriginX;
    double mOriginY;
    double mOriginZ;
};
