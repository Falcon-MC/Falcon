#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <cstdint>
#include <vector>

class SurfaceOverwriteBuilder {
public:
    static constexpr int32_t SEA_LEVEL = 63;
    static constexpr int32_t CLAY_BANDS_SIZE = 192;

    SurfaceOverwriteBuilder(const XoroshiroPositionalFactory &noiseFactory, int64_t levelSeed);

    void apply(LevelChunk &chunk) const;

private:
    void _erodedBadlandsExtension(LevelChunk &chunk, int32_t localX, int32_t localZ, int32_t worldX, int32_t worldZ,
                                  int32_t height) const;

    void _applyClayBandsDepth(LevelChunk &chunk, int32_t localX, int32_t localZ, int32_t worldX, int32_t worldZ,
                              int32_t surfaceY) const;

    const BlockState &_getClayBand(int32_t worldX, int32_t y, int32_t worldZ) const;

    void _generateBands(int64_t seed);

    void _makeBands(IRandom &random, int32_t baseWidth, const BlockState &state);

    static bool _isWaterState(const BlockState &state);

    static bool _isSolidState(const BlockState &state);

    static bool _isInRange(float noise);

    static int32_t _javaStringHash(const char *text);

    NormalNoisePtr mSurfaceNoise;
    NormalNoisePtr mClayBandsOffsetNoise;
    NormalNoisePtr mBadlandsPillarNoise;
    NormalNoisePtr mBadlandsPillarRoofNoise;
    NormalNoisePtr mBadlandsSurfaceNoise;
    std::vector<BlockState> mClayBandsCache;
};
