#include "World/Level/LevelGen/Overworld/Terrain/LatticeNoise.h"

#include "World/Level/LevelGen/Noise/NoiseMath.h"

#include <cmath>

namespace {

    const float GRADIENTS[16][3] = {{1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f},
                                    {1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, -1.0f},
                                    {0.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 1.0f}, {0.0f, 1.0f, -1.0f}, {0.0f, -1.0f, -1.0f},
                                    {1.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 0.0f}, {0.0f, -1.0f, -1.0f}};

    float gradient(int32_t hash, float x, float y, float z) {
        const float *g = GRADIENTS[hash & 15];
        return g[2] * z + (g[1] * y + g[0] * x);
    }

    float fadePolynomial(float t) {
        return (t * 6.0f + -15.0f) * t + 10.0f;
    }

}

LatticeNoise::LatticeNoise(XoroshiroRandom &random) : mPermutation() {
    const float x = random.nextFloat();
    const float y = random.nextFloat();
    const float z = random.nextFloat();
    mOriginX = x * 256.0f;
    mOriginY = y * 256.0f;
    mOriginZ = z * 256.0f;

    for (int32_t i = 0; i < 256; i++)
        mPermutation[static_cast<size_t>(i)] = i;

    for (int32_t i = 0; i < 256; i++) {
        const int32_t j = i + random.nextExclusiveInt(256 - i);
        const int32_t swapped = mPermutation[static_cast<size_t>(i)];
        mPermutation[static_cast<size_t>(i)] = mPermutation[static_cast<size_t>(j)];
        mPermutation[static_cast<size_t>(j)] = swapped;
        mPermutation[static_cast<size_t>(i + 256)] = mPermutation[static_cast<size_t>(i)];
    }
}

void LatticeNoise::addLattice(float *output, const float *start, int32_t countX, int32_t countY, int32_t countZ,
                              const float *scale, float frequency) const {
    const float amplitude = 1.0f / frequency;

    // y lattice points in the origin's own unit keep the origin's fraction instead of their own
    const float originY = scale[1] * 0.0f + mOriginY;
    const int32_t originSection = NoiseMath::truncateToInt(originY);
    const float originFraction = originY - static_cast<float>(NoiseMath::truncateToInt(originY));

    // set once per call like the game (sub_145C2A210): a column can reuse the previous column's corners
    int32_t cachedSection = -1;
    float corners[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    size_t index = 0;

    for (int32_t ix = 0; ix < countX; ix++) {
        const float fx = (static_cast<float>(ix) + start[0]) * scale[0] + mOriginX;
        const float floorX = std::floor(fx);
        const int32_t sectionX = NoiseMath::truncateToInt(floorX) & 0xFF;
        const float localX = fx - floorX;
        const float fadeX = fadePolynomial(localX) * (localX * localX * localX);
        const float localX1 = localX + -1.0f;

        for (int32_t iz = 0; iz < countZ; iz++) {
            const float fz = (static_cast<float>(iz) + start[2]) * scale[2] + mOriginZ;
            const float floorZ = std::floor(fz);
            const int32_t sectionZ = NoiseMath::truncateToInt(floorZ) & 0xFF;
            const float localZ = fz - floorZ;
            const float fadeZ = fadePolynomial(localZ) * (localZ * localZ * localZ);
            const float localZ1 = localZ + -1.0f;

            for (int32_t iy = 0; iy < countY; iy++) {
                const float fy = (static_cast<float>(iy) + start[1]) * scale[1] + mOriginY;
                const float floorY = std::floor(fy);
                const int32_t sectionY = NoiseMath::truncateToInt(floorY) & 0xFF;
                const float localY = fy - floorY;

                bool compute = true;
                float gradientY = originFraction;
                if (originSection != NoiseMath::truncateToInt(fy)) {
                    if (sectionY == cachedSection) {
                        // the game reuses the corner values of the first point of a y section
                        compute = false;
                    } else {
                        gradientY = localY - std::floor(localY / scale[1]) * scale[1];
                    }
                }

                if (compute) {
                    const int32_t a = _permutation(sectionX) + sectionY;
                    const int32_t b = _permutation(sectionX + 1) + sectionY;
                    const int32_t aa = _permutation(a) + sectionZ;
                    const int32_t ab = _permutation(a + 1) + sectionZ;
                    const int32_t ba = _permutation(b) + sectionZ;
                    const int32_t bb = _permutation(b + 1) + sectionZ;
                    const float gradientY1 = gradientY + -1.0f;

                    const float low[4] = {gradient(_permutation(ab), localX, gradientY1, localZ),
                                          gradient(_permutation(aa + 1), localX, gradientY, localZ1),
                                          gradient(_permutation(aa), localX, gradientY, localZ),
                                          gradient(_permutation(ab + 1), localX, gradientY1, localZ1)};
                    const float high[4] = {gradient(_permutation(bb), localX1, gradientY1, localZ),
                                           gradient(_permutation(ba + 1), localX1, gradientY, localZ1),
                                           gradient(_permutation(ba), localX1, gradientY, localZ),
                                           gradient(_permutation(bb + 1), localX1, gradientY1, localZ1)};
                    for (int32_t lane = 0; lane < 4; lane++)
                        corners[lane] = (high[lane] - low[lane]) * fadeX + low[lane];

                    cachedSection = sectionY;
                }

                const float fadeY = (localY * localY * localY) * fadePolynomial(localY);
                const float farZ = (corners[3] - corners[1]) * fadeY + corners[1];
                const float nearZ = (corners[0] - corners[2]) * fadeY + corners[2];
                const float value = (farZ - nearZ) * fadeZ + nearZ;
                output[index] = value * amplitude + output[index];
                index++;
            }
        }
    }
}
