#include "World/Level/LevelGen/Overworld/Biome/BiomeZoom.h"

#include <cstddef>

namespace {

    constexpr uint64_t LCG_MULTIPLIER = 0x5DEECE66DULL;
    constexpr uint64_t LCG_ADDEND = 11ULL;

    float jitter(uint64_t state) {
        return (static_cast<float>((state >> 24) & 0xFFFFFFULL) * 5.9604645e-8f + -0.5f) * 0.9f;
    }

    float square(float value) {
        return value * value;
    }

}

BiomeZoom::BiomeZoom(uint32_t seed, int32_t originX, int32_t originY, int32_t originZ, int32_t countX, int32_t countY,
                     int32_t countZ)
        : mOriginX(originX), mOriginY(originY), mOriginZ(originZ), mCountY(countY), mCountZ(countZ) {
    const uint64_t scrambled = static_cast<uint64_t>(seed) ^ LCG_MULTIPLIER;
    mJitter.reserve(static_cast<size_t>(countX * countY * countZ));
    for (int32_t indexX = 0; indexX < countX; indexX++) {
        const int32_t hashX = static_cast<int32_t>(3129871U * static_cast<uint32_t>((originX + 8 * indexX) >> 3));
        for (int32_t indexZ = 0; indexZ < countZ; indexZ++) {
            const int64_t hashZ = 116129781LL * static_cast<int64_t>((originZ + 8 * indexZ) >> 3);
            for (int32_t indexY = 0; indexY < countY; indexY++) {
                const uint64_t hash = static_cast<uint64_t>(hashZ ^ static_cast<int64_t>(hashX)
                                                            ^ static_cast<int64_t>((originY + 8 * indexY) >> 3));
                const uint64_t mixed = (hash * (42317861ULL * hash + 11ULL)) >> 16;
                const uint64_t first = LCG_MULTIPLIER * (scrambled ^ mixed) + LCG_ADDEND;
                const uint64_t second = LCG_MULTIPLIER * first + LCG_ADDEND;
                const uint64_t third = LCG_MULTIPLIER * second + LCG_ADDEND;
                mJitter.push_back(Jitter{jitter(first), jitter(second), jitter(third)});
            }
        }
    }
}

int32_t BiomeZoom::nearestSample(int32_t x, int32_t y, int32_t z) const {
    const float fractionX = static_cast<float>(x & 7) * 0.125f;
    const float fractionY = static_cast<float>(y & 7) * 0.125f;
    const float fractionZ = static_cast<float>(z & 7) * 0.125f;
    const int32_t base = sampleIndex((x >> 3) - (mOriginX >> 3), (y >> 3) - (mOriginY >> 3),
                                     (z >> 3) - (mOriginZ >> 3));

    // corners in the game's order: z, then y, then x
    static const int32_t CORNERS[8][3] = {{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0, 1, 1},
                                          {1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}};

    int32_t best = base;
    float bestDistance = 0.0f;
    for (int32_t i = 0; i < 8; i++) {
        const int32_t index = base + CORNERS[i][0] * mCountZ * mCountY + CORNERS[i][2] * mCountY + CORNERS[i][1];
        const Jitter &offset = mJitter[static_cast<size_t>(index)];
        const float dx = offset.mX + (fractionX - static_cast<float>(CORNERS[i][0]));
        const float dy = offset.mY + (fractionY - static_cast<float>(CORNERS[i][1]));
        const float dz = offset.mZ + (fractionZ - static_cast<float>(CORNERS[i][2]));
        const float distance = square(dz) + (square(dx) + square(dy));
        if (i == 0 || distance < bestDistance) {
            best = index;
            bestDistance = distance;
        }
    }

    return best;
}
