#include "World/Level/LevelGen/Overworld/Surface/SurfaceSimplexNoise.h"

#include "World/Level/LevelGen/Noise/NoiseMath.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
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
        return NoiseMath::truncateToInt(value) - (value > 0.0f ? 0 : 1);
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

SurfaceSimplexNoise::SurfaceSimplexNoise(std::mt19937 &random) {
    // the z offset comes first here, and a 2D octave never reads it
    GameRandom::nextFloat(random);
    mOffsetY = GameRandom::nextFloat(random) * 256.0f;
    mOffsetX = GameRandom::nextFloat(random) * 256.0f;

    for (int32_t i = 0; i < 256; i++)
        mPermutation[(size_t) i] = i;

    for (int32_t i = 0; i < 256; i++) {
        const int32_t other = i + GameRandom::nextInt(random, 256 - i);
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

float SurfaceSimplexNoise::getValue3D(float x, float y, float z) const {
    static const float GRADIENTS_3D[12][3] = {
            {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f},
            {1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 1.0f}, {0.0f, 1.0f, -1.0f}, {0.0f, -1.0f, -1.0f}
    };

    const float skew = (z + (x + y)) * 0.33333334f;
    const int32_t i = gameFloor(x + skew);
    const int32_t j = gameFloor(y + skew);
    const int32_t k = gameFloor(z + skew);
    const float unskew = static_cast<float>(k + i + j) * 0.16666667f;
    const float x0 = x - (static_cast<float>(i) - unskew);
    const float y0 = y - (static_cast<float>(j) - unskew);
    const float z0 = z - (static_cast<float>(k) - unskew);

    int32_t i1 = 0, j1 = 0, k1 = 0, i2 = 0, j2 = 0, k2 = 0;
    if (x0 < y0) {
        j2 = 1;
        if (z0 > y0) {
            k1 = 1;
            k2 = 1;
        } else if (z0 > x0) {
            j1 = 1;
            k2 = 1;
        } else {
            j1 = 1;
            i2 = 1;
        }
    } else {
        i2 = 1;
        if (y0 >= z0) {
            i1 = 1;
            j2 = 1;
        } else if (x0 >= z0) {
            i1 = 1;
            k2 = 1;
        } else {
            k1 = 1;
            k2 = 1;
        }
    }

    const auto corner3D = [&](int32_t di, int32_t dj, int32_t dk, float cx, float cy, float cz) {
        const float falloff = ((0.6f - cx * cx) - cy * cy) - cz * cz;
        if (falloff < 0.0f)
            return 0.0f;

        const int32_t inner = mPermutation[(size_t) ((k + dk) & 255)] + ((j + dj) & 255);
        const int32_t outer = mPermutation[(size_t) inner] + ((i + di) & 255);
        const float *g = GRADIENTS_3D[mPermutation[(size_t) outer] % 12];
        return (g[2] * cz + (g[1] * cy + g[0] * cx)) * (((falloff * falloff) * falloff) * falloff);
    };

    const float first = corner3D(0, 0, 0, x0, y0, z0);
    const float second = corner3D(i1, j1, k1, (x0 - (float) i1) + 0.16666667f, (y0 - (float) j1) + 0.16666667f,
                                  (z0 - (float) k1) + 0.16666667f);
    const float third = corner3D(i2, j2, k2, (x0 - (float) i2) + 0.33333334f, (y0 - (float) j2) + 0.33333334f,
                                 (z0 - (float) k2) + 0.33333334f);
    const float fourth = corner3D(1, 1, 1, (x0 + -1.0f) + 0.5f, (y0 + -1.0f) + 0.5f, (z0 + -1.0f) + 0.5f);

    return (((first + second) + third) + fourth) * 32.0f;
}
