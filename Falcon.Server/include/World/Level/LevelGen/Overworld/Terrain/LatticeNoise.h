#pragma once

#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <array>
#include <cstdint>

// octave of the blended terrain noise, sampled over a whole lattice
class LatticeNoise {
public:
    explicit LatticeNoise(XoroshiroRandom &random);

    // adds value / frequency at (start + (i, j, k)) * scale for every lattice point, y varying fastest
    void addLattice(float *output, const float *start, int32_t countX, int32_t countY, int32_t countZ,
                    const float *scale, float frequency) const;

private:
    int32_t _permutation(int32_t index) const {
        return mPermutation[static_cast<size_t>(index)];
    }

    float mOriginX;
    float mOriginY;
    float mOriginZ;
    std::array<int32_t, 512> mPermutation;
};
