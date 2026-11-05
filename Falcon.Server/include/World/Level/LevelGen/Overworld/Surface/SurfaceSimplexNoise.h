#pragma once

#include <array>
#include <cstdint>

class XoroshiroRandom;

// one 2D simplex octave of the overworld surface noise, in float like the game (sub_145C36410, sub_145C2DA70)
class SurfaceSimplexNoise {
public:
    explicit SurfaceSimplexNoise(XoroshiroRandom &random);

    float getValue(float x, float y) const;

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
