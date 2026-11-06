#include "World/Level/LevelGen/End/Noise/TheEndSimplexNoise.h"

#include <cmath>

namespace {

    const float F2 = (float) (0.5 * (std::sqrt(3.0) - 1.0));
    const float G2 = (float) ((3.0 - std::sqrt(3.0)) / 6.0);

    const float GRADIENTS[12][2] = {
            {1.0f,  1.0f},
            {-1.0f, 1.0f},
            {1.0f,  -1.0f},
            {-1.0f, -1.0f},
            {1.0f,  0.0f},
            {-1.0f, 0.0f},
            {1.0f,  0.0f},
            {-1.0f, 0.0f},
            {0.0f,  1.0f},
            {0.0f,  -1.0f},
            {0.0f,  1.0f},
            {0.0f,  -1.0f}
    };

    int32_t fastFloor(float value) {
        return value > 0.0f ? (int32_t) value : (int32_t) value - 1;
    }

    float contribution(int32_t gradient, float x, float z) {
        const float falloff = (0.5f - x * x) - z * z;
        if (falloff < 0.0f)
            return 0.0f;

        const float *vector = GRADIENTS[gradient % 12];
        return (vector[1] * z + vector[0] * x) * (((falloff * falloff) * falloff) * falloff);
    }

}

TheEndSimplexNoise::TheEndSimplexNoise(std::mt19937 &random)
        : mPermutations() {
    for (int32_t i = 0; i < 3; i++)
        random();

    for (int32_t i = 0; i < 256; i++)
        mPermutations[(size_t) i] = i;

    for (int32_t i = 0; i < 256; i++) {
        const int32_t swap = (int32_t) (random() % (uint32_t) (256 - i)) + i;
        const int32_t value = mPermutations[(size_t) i];
        mPermutations[(size_t) i] = mPermutations[(size_t) swap];
        mPermutations[(size_t) swap] = value;
        mPermutations[(size_t) (i + 256)] = mPermutations[(size_t) i];
    }
}

float TheEndSimplexNoise::getValue(float x, float z) const {
    const float skew = (z + x) * F2;
    const int32_t i = fastFloor(skew + x);
    const int32_t j = fastFloor(skew + z);
    const float unskew = (float) (i + j) * G2;
    const float x0 = x - ((float) i - unskew);
    const float z0 = z - ((float) j - unskew);

    const int32_t stepX = x0 > z0 ? 1 : 0;
    const int32_t stepZ = 1 - stepX;
    const float x1 = G2 + (x0 - (float) stepX);
    const float z1 = G2 + (z0 - (float) stepZ);
    const float x2 = (G2 + G2) + (x0 + -1.0f);
    const float z2 = (G2 + G2) + (z0 + -1.0f);

    const int32_t latticeX = i & 255;
    const int32_t latticeZ = j & 255;
    const std::array<int32_t, 512> &p = mPermutations;
    const float first = contribution(p[(size_t) (latticeX + p[(size_t) latticeZ])], x0, z0);
    const float second = contribution(p[(size_t) (stepX + latticeX + p[(size_t) (stepZ + latticeZ)])], x1, z1);
    const float third = contribution(p[(size_t) (p[(size_t) latticeZ + 1] + latticeX + 1)], x2, z2);
    return ((first + second) + third) * 70.0f;
}
