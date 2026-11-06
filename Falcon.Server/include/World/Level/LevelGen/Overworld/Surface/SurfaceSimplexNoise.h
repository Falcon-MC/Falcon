#pragma once

#include <array>
#include <cstdint>
#include <random>

class XoroshiroRandom;

// one 2D simplex octave of the overworld surface noise, in float like the game (sub_145C36410, sub_145C2DA70)
class SurfaceSimplexNoise {
public:
    explicit SurfaceSimplexNoise(XoroshiroRandom &random);

    // drawn from the game's Mersenne twister Random instead, offsets in the other order (sub_145C2D720)
    explicit SurfaceSimplexNoise(std::mt19937 &random);

    float getValue(float x, float y) const;

    // the 3D form reads the position as it is, without the octave's offsets (sub_145C2DCF0)
    float getValue3D(float x, float y, float z) const;

    float getOffsetX() const {
        return mOffsetX;
    }

    float getOffsetY() const {
        return mOffsetY;
    }

private:
    float mOffsetX;
    float mOffsetY;
    std::array<int32_t, 512> mPermutation;
};
