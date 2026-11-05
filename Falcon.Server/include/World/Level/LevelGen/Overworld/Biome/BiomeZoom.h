#pragma once

#include <cstdint>
#include <vector>

// spreads biomes sampled every 8 blocks to single blocks through jittered nearest samples (sub_14A227A70)
class BiomeZoom {
public:
    // samples cover countX * countZ * countY points 8 blocks apart from origin, y varying fastest
    BiomeZoom(uint32_t seed, int32_t originX, int32_t originY, int32_t originZ, int32_t countX, int32_t countY,
              int32_t countZ);

    // index of the sample whose jittered position is closest to the block (sub_1484FDF90)
    int32_t nearestSample(int32_t x, int32_t y, int32_t z) const;

    int32_t sampleIndex(int32_t indexX, int32_t indexY, int32_t indexZ) const {
        return (indexX * mCountZ + indexZ) * mCountY + indexY;
    }

private:
    struct Jitter {
        float mX;
        float mY;
        float mZ;
    };

    int32_t mOriginX;
    int32_t mOriginY;
    int32_t mOriginZ;
    int32_t mCountY;
    int32_t mCountZ;
    std::vector<Jitter> mJitter;
};
