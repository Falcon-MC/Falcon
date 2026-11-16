#pragma once

#include "World/Level/LevelGen/Noise/NormalNoise.h"

#include <array>
#include <cstdint>
#include <random>

class LevelChunk;
class SurfaceBuilderComponent;

// minecraft:mesa builder: clay bands, red sand and the eroded badlands pillars
class MesaSurfaceBuilder {
public:
    explicit MesaSurfaceBuilder(int64_t levelSeed);

    // noise is the column's surface noise; random is the chunk's, one draw per column
    void build(LevelChunk &chunk, int32_t x, int32_t z, int32_t lowestY, const SurfaceBuilderComponent &component,
               float noise, std::mt19937 &random) const;

private:
    static constexpr int32_t BAND_COUNT = 192;

    // the terracotta color of a band, above 15 for hardened clay
    uint8_t _band(int32_t worldX, int32_t y, int32_t worldZ) const;

    // the top of an eroded badlands pillar, 0 without one
    float _pillarTop(int32_t worldX, int32_t worldZ) const;

    std::array<uint8_t, BAND_COUNT> mBands{};
    NormalNoisePtr mClayBandsOffsetNoise;
    NormalNoisePtr mSurfaceNoise;
    NormalNoisePtr mPillarNoise;
    NormalNoisePtr mPillarRoofNoise;
};
