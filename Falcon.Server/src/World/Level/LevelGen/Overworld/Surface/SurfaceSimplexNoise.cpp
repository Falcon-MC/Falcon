#include "World/Level/LevelGen/Overworld/Surface/SurfaceSimplexNoise.h"

#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <utility>

namespace {

    // float constants written by sub_145C3E360
    const float SKEW = 0.3660253882408142f;
    const float UNSKEW = 0.21132487058639526f;

    // gradients copied from 0x14F279F40, only x and y are read in 2D
    const float GRADIENTS[12][2] = {
            {1.0f, 1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f},
            {1.0f, 0.0f}, {-1.0f, 0.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f},
            {0.0f, 1.0f}, {0.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}
    };

    // the game subtracts one from every value that is not above zero, integers included
    int32_t gameFloor(float value) {
        return static_cast<int32_t>(value) - (value > 0.0f ? 0 : 1);
    }

    float corner(int32_t gradient, float x, float y) {
        const float falloff = (0.5f - x * x) - y * y;
        if (falloff < 0.0f)
            return 0.0f;

        const float *g = GRADIENTS[gradient % 12];
        return (y * g[1] + x * g[0]) * (((falloff * falloff) * falloff) * falloff);
    }

}

SurfaceSimplexNoise::SurfaceSimplexNoise(XoroshiroRandom &random) {
    mOffsetX = random.nextFloat() * 256.0f;
    mOffsetY = random.nextFloat() * 256.0f;
    // the z offset is drawn but a 2D octave never reads it
    random.nextFloat();

    for (int32_t i = 0; i < 256; i++)
        mPermutation[(size_t) i] = i;

    for (int32_t i = 0; i < 256; i++) {
        const int32_t other = i + random.nextExclusiveInt(256 - i);
        std::swap(mPermutation[(size_t) i], mPermutation[(size_t) other]);
        mPermutation[(size_t) i + 256] = mPermutation[(size_t) i];
    }
}

float SurfaceSimplexNoise::getValue(float x, float y) const {
    const float skew = (y + x) * SKEW;
    const int32_t i = gameFloor(x + skew);
    const int32_t j = gameFloor(y + skew);
    const float unskew = static_cast<float>(i + j) * UNSKEW;
    const float x0 = x - (static_cast<float>(i) - unskew);
    const float y0 = y - (static_cast<float>(j) - unskew);

    const int32_t i1 = x0 > y0 ? 1 : 0;
    const int32_t j1 = 1 - i1;
    const float x1 = UNSKEW + (x0 - static_cast<float>(i1));
    const float y1 = UNSKEW + (y0 - static_cast<float>(j1));
    const float x2 = (UNSKEW + UNSKEW) + (x0 + -1.0f);
    const float y2 = (UNSKEW + UNSKEW) + (y0 + -1.0f);

    const int32_t ii = i & 255;
    const int32_t jj = j & 255;
    const int32_t g0 = mPermutation[(size_t) (ii + mPermutation[(size_t) jj])];
    const int32_t g1 = mPermutation[(size_t) (ii + i1 + mPermutation[(size_t) (jj + j1)])];
    const int32_t g2 = mPermutation[(size_t) (ii + 1 + mPermutation[(size_t) (jj + 1)])];

    return ((corner(g0, x0, y0) + corner(g1, x1, y1)) + corner(g2, x2, y2)) * 70.0f;
}
