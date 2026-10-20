#include "World/Level/LevelGen/End/Noise/TheEndSimplexNoise.h"

#include <cmath>

namespace {

    const double SQRT_3 = std::sqrt(3.0);
    const double F2 = 0.5 * (SQRT_3 - 1.0);
    const double G2 = (3.0 - SQRT_3) / 6.0;

    const int32_t GRAD3[12][3] = {
            {1,  1,  0},
            {-1, 1,  0},
            {1,  -1, 0},
            {-1, -1, 0},
            {1,  0,  1},
            {-1, 0,  1},
            {1,  0,  -1},
            {-1, 0,  -1},
            {0,  1,  1},
            {0,  -1, 1},
            {0,  1,  -1},
            {0,  -1, -1}
    };

}

TheEndSimplexNoise::TheEndSimplexNoise(IRandom &random)
        : mPermutations(), mOriginX(0.0), mOriginY(0.0), mOriginZ(0.0) {
    mPermutations.fill(0);

    mOriginX = random.nextDouble() * 256.0;
    mOriginY = random.nextDouble() * 256.0;
    mOriginZ = random.nextDouble() * 256.0;

    for (int32_t i = 0; i < 256; i++)
        mPermutations[(size_t) i] = i;

    for (int32_t l = 0; l < 256; l++) {
        const int32_t j = random.nextBoundedInt(256 - l) + l;
        const int32_t k = mPermutations[(size_t) l];
        mPermutations[(size_t) l] = mPermutations[(size_t) j];
        mPermutations[(size_t) j] = k;
        mPermutations[(size_t) (l + 256)] = mPermutations[(size_t) l];
    }
}

int32_t TheEndSimplexNoise::_fastFloor(double value) {
    return value > 0.0 ? (int32_t) value : (int32_t) value - 1;
}

double TheEndSimplexNoise::_dot(const int32_t *gradient, double x, double z) {
    return (double) gradient[0] * x + (double) gradient[1] * z;
}

double TheEndSimplexNoise::getValue(double x, double z) const {
    const double d4 = (x + z) * F2;
    const int32_t i = _fastFloor(x + d4);
    const int32_t j = _fastFloor(z + d4);
    const double d6 = (double) (i + j) * G2;
    const double d7 = (double) i - d6;
    const double d8 = (double) j - d6;
    const double d9 = x - d7;
    const double d10 = z - d8;

    int32_t k = 0;
    int32_t l = 0;
    if (d9 > d10) {
        k = 1;
        l = 0;
    } else {
        k = 0;
        l = 1;
    }

    const double d11 = d9 - (double) k + G2;
    const double d12 = d10 - (double) l + G2;
    const double d13 = d9 - 1.0 + 2.0 * G2;
    const double d14 = d10 - 1.0 + 2.0 * G2;
    const int32_t i1 = i & 255;
    const int32_t j1 = j & 255;
    const int32_t k1 = mPermutations[(size_t) (i1 + mPermutations[(size_t) j1])] % 12;
    const int32_t l1 = mPermutations[(size_t) (i1 + k + mPermutations[(size_t) (j1 + l)])] % 12;
    const int32_t i2 = mPermutations[(size_t) (i1 + 1 + mPermutations[(size_t) (j1 + 1)])] % 12;

    double d15 = 0.5 - d9 * d9 - d10 * d10;
    double d0 = 0.0;
    if (d15 < 0.0) {
        d0 = 0.0;
    } else {
        d15 = d15 * d15;
        d0 = d15 * d15 * _dot(GRAD3[k1], d9, d10);
    }

    double d16 = 0.5 - d11 * d11 - d12 * d12;
    double d1 = 0.0;
    if (d16 < 0.0) {
        d1 = 0.0;
    } else {
        d16 = d16 * d16;
        d1 = d16 * d16 * _dot(GRAD3[l1], d11, d12);
    }

    double d17 = 0.5 - d13 * d13 - d14 * d14;
    double d2 = 0.0;
    if (d17 < 0.0) {
        d2 = 0.0;
    } else {
        d17 = d17 * d17;
        d2 = d17 * d17 * _dot(GRAD3[i2], d13, d14);
    }

    return 70.0 * (d0 + d1 + d2);
}
