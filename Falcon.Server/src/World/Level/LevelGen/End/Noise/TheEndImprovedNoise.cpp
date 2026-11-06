#include "World/Level/LevelGen/End/Noise/TheEndImprovedNoise.h"

#include <cmath>

namespace {

    const float GRADIENTS[16][3] = {
            {1.0f,  1.0f,  0.0f},
            {-1.0f, 1.0f,  0.0f},
            {1.0f,  -1.0f, 0.0f},
            {-1.0f, -1.0f, 0.0f},
            {1.0f,  0.0f,  1.0f},
            {-1.0f, 0.0f,  1.0f},
            {1.0f,  0.0f,  -1.0f},
            {-1.0f, 0.0f,  -1.0f},
            {0.0f,  1.0f,  1.0f},
            {0.0f,  -1.0f, 1.0f},
            {0.0f,  1.0f,  -1.0f},
            {0.0f,  -1.0f, -1.0f},
            {1.0f,  1.0f,  0.0f},
            {0.0f,  -1.0f, 1.0f},
            {-1.0f, 1.0f,  0.0f},
            {0.0f,  -1.0f, -1.0f}
    };

    float nextUnit(std::mt19937 &random) {
        return static_cast<float>(static_cast<double>(random()) * 2.3283064365386963e-10);
    }

    float grad(int32_t hash, float x, float y, float z) {
        const float *gradient = GRADIENTS[hash & 15];
        return gradient[2] * z + (gradient[1] * y + gradient[0] * x);
    }

    float fade(float value) {
        return ((value * 6.0f + -15.0f) * value + 10.0f) * ((value * value) * value);
    }

}

TheEndImprovedNoise::TheEndImprovedNoise(std::mt19937 &random)
        : mPermutations(), mXCoord(0.0f), mYCoord(0.0f), mZCoord(0.0f) {
    mXCoord = nextUnit(random) * 256.0f;
    mYCoord = nextUnit(random) * 256.0f;
    mZCoord = nextUnit(random) * 256.0f;

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

void TheEndImprovedNoise::addOctave(float *noiseArray, const float *position, int32_t xSize, int32_t ySize,
                                    int32_t zSize, const float *scale, float weight) const {
    const std::array<int32_t, 512> &p = mPermutations;
    const float inverseWeight = 1.0f / weight;

    // the layer at the noise origin keeps its own fraction, the others snap down to a multiple of the y scale
    const int32_t originY = (int32_t) mYCoord;
    const float originFraction = mYCoord - (float) originY;

    float lanes[4] = {};
    int32_t cachedLattice = -1;
    int32_t index = 0;

    for (int32_t ix = 0; ix < xSize; ix++) {
        const float x = ((float) ix + position[0]) * scale[0] + mXCoord;
        const float floorX = std::floor(x);
        const int32_t latticeX = (int32_t) (uint8_t) (int32_t) floorX;
        const float fractionX = x - floorX;
        const float fadeX = fade(fractionX);

        for (int32_t iz = 0; iz < zSize; iz++) {
            const float z = ((float) iz + position[2]) * scale[2] + mZCoord;
            const float floorZ = std::floor(z);
            const int32_t latticeZ = (int32_t) (uint8_t) (int32_t) floorZ;
            const float fractionZ = z - floorZ;
            const float fadeZ = fade(fractionZ);

            for (int32_t iy = 0; iy < ySize; iy++) {
                const float y = ((float) iy + position[1]) * scale[1] + mYCoord;
                const float floorY = std::floor(y);
                const int32_t latticeY = (int32_t) (uint8_t) (int32_t) floorY;
                const float fractionY = y - floorY;

                bool refresh = true;
                float gradientY = originFraction;
                if (originY != (int32_t) y) {
                    refresh = latticeY != cachedLattice;
                    gradientY = fractionY - std::floor(fractionY / scale[1]) * scale[1];
                }

                if (refresh) {
                    cachedLattice = latticeY;

                    const int32_t a = latticeY + p[(size_t) latticeX];
                    const int32_t b = latticeY + p[(size_t) latticeX + 1];
                    const int32_t aa = latticeZ + p[(size_t) a];
                    const int32_t ab = latticeZ + p[(size_t) a + 1];
                    const int32_t ba = latticeZ + p[(size_t) b];
                    const int32_t bb = latticeZ + p[(size_t) b + 1];
                    const float x1 = fractionX + -1.0f;
                    const float y1 = gradientY + -1.0f;
                    const float z1 = fractionZ + -1.0f;

                    const float low[4] = {grad(p[(size_t) ab], fractionX, y1, fractionZ),
                                          grad(p[(size_t) aa + 1], fractionX, gradientY, z1),
                                          grad(p[(size_t) aa], fractionX, gradientY, fractionZ),
                                          grad(p[(size_t) ab + 1], fractionX, y1, z1)};
                    const float high[4] = {grad(p[(size_t) bb], x1, y1, fractionZ),
                                           grad(p[(size_t) ba + 1], x1, gradientY, z1),
                                           grad(p[(size_t) ba], x1, gradientY, fractionZ),
                                           grad(p[(size_t) bb + 1], x1, y1, z1)};
                    for (int32_t lane = 0; lane < 4; lane++)
                        lanes[lane] = (high[lane] - low[lane]) * fadeX + low[lane];
                }

                const float fadeY = ((fractionY * fractionY) * fractionY)
                                    * ((fractionY * 6.0f + -15.0f) * fractionY + 10.0f);
                const float far = fadeY * (lanes[3] - lanes[1]) + lanes[1];
                const float near = fadeY * (lanes[0] - lanes[2]) + lanes[2];
                noiseArray[index] = ((far - near) * fadeZ + near) * inverseWeight + noiseArray[index];
                index++;
            }
        }
    }
}

float TheEndImprovedNoise::sample(float x, float y, float z) const {
    const std::array<int32_t, 512> &p = mPermutations;
    const float floorX = std::floor(x);
    const float floorY = std::floor(y);
    const float floorZ = std::floor(z);
    const int32_t latticeX = (int32_t) (uint8_t) (int32_t) floorX;
    const int32_t latticeY = (int32_t) (uint8_t) (int32_t) floorY;
    const int32_t latticeZ = (int32_t) (uint8_t) (int32_t) floorZ;
    const float fx = x - floorX;
    const float fy = y - floorY;
    const float fz = z - floorZ;
    const float x1 = fx + -1.0f;
    const float y1 = fy + -1.0f;
    const float z1 = fz + -1.0f;

    const int32_t a = latticeY + p[(size_t) latticeX];
    const int32_t b = latticeY + p[(size_t) latticeX + 1];
    const int32_t aa = latticeZ + p[(size_t) a];
    const int32_t ab = latticeZ + p[(size_t) a + 1];
    const int32_t ba = latticeZ + p[(size_t) b];
    const int32_t bb = latticeZ + p[(size_t) b + 1];

    const float fadeX = fade(fx);
    const float fadeY = fade(fy);
    const float fadeZ = fade(fz);
    const auto lerp = [](float t, float from, float to) { return (to - from) * t + from; };

    const float nearLow = lerp(fadeX, grad(p[(size_t) aa], fx, fy, fz), grad(p[(size_t) ba], x1, fy, fz));
    const float nearHigh = lerp(fadeX, grad(p[(size_t) ab], fx, y1, fz), grad(p[(size_t) bb], x1, y1, fz));
    const float farLow = lerp(fadeX, grad(p[(size_t) aa + 1], fx, fy, z1), grad(p[(size_t) ba + 1], x1, fy, z1));
    const float farHigh = lerp(fadeX, grad(p[(size_t) ab + 1], fx, y1, z1), grad(p[(size_t) bb + 1], x1, y1, z1));

    return lerp(fadeZ, lerp(fadeY, nearLow, nearHigh), lerp(fadeY, farLow, farHigh));
}
