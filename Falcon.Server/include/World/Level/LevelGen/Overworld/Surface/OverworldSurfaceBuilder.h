#pragma once

#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceSimplexNoise.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

#include <random>
#include <vector>

class BlockState;
class LevelChunk;
class OverworldClimate;
class SurfaceBuilderComponent;

class OverworldSurfaceBuilder {
public:
    OverworldSurfaceBuilder(const XoroshiroPositionalFactory &noiseFactory, const OverworldClimate &climate);

    // random is the chunk's random, already past the bedrock floor
    void apply(LevelChunk &chunk, std::mt19937 &random) const;

private:
    void _fillSurfaceNoise(int32_t chunkX, int32_t chunkZ, float *noise) const;

    // the column's own materials with Falcon's noise ranges, not yet the game's adjustments
    void _adjustMaterials(const SurfaceBuilderComponent &component, int32_t worldX, int32_t worldZ,
                          const BlockState *&top, const BlockState *&mid, const BlockState *&seaFloor) const;

    void _buildOverworldColumn(LevelChunk &chunk, int32_t x, int32_t z, int32_t height,
                               const SurfaceBuilderComponent &component, float noise, std::mt19937 &random) const;

    // builders other than the overworld one, not ported yet
    void _buildLegacyColumn(LevelChunk &chunk, int32_t x, int32_t z, const SurfaceBuilderComponent &component) const;

    const OverworldClimate &mClimate;
    std::vector<SurfaceSimplexNoise> mSurfaceNoise;
    NormalNoisePtr mNoise;
};
