#pragma once

#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Overworld/Surface/NoiseGradientSurfaceBuilder.h"
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
    OverworldSurfaceBuilder(const XoroshiroPositionalFactory &noiseFactory, const OverworldClimate &climate,
                            int64_t levelSeed);

    // random is the chunk's random, already past the bedrock floor
    void apply(LevelChunk &chunk, std::mt19937 &random) const;

private:
    void _fillSurfaceNoise(int32_t chunkX, int32_t chunkZ, float *noise) const;

    // the column's materials after the biome's surface_material_adjustments
    void _adjustMaterials(const SurfaceBuilderComponent &component, int32_t worldX, int32_t worldZ,
                          const BlockState *&top, const BlockState *&mid, const BlockState *&seaFloor) const;

    void _buildOverworldColumn(LevelChunk &chunk, int32_t x, int32_t z, int32_t height,
                               const SurfaceBuilderComponent &component, float noise, std::mt19937 &random) const;

    void _buildIcebergColumn(LevelChunk &chunk, int32_t x, int32_t z, int32_t height) const;

    void _digSwampPuddle(LevelChunk &chunk, int32_t x, int32_t z, int32_t height, int32_t maxDepth) const;

    const NoiseGradientSurfaceBuilder *_noiseGradientFor(int32_t biome) const;

    // after every column has its surface
    void _buildSubsurface(LevelChunk &chunk) const;

    // the lowest block a column's builders may change
    int32_t _lowestSurfaceY(int32_t worldX, int32_t worldZ) const;

    // builders other than the overworld one, not ported yet
    void _buildLegacyColumn(LevelChunk &chunk, int32_t x, int32_t z, const SurfaceBuilderComponent &component) const;

    const OverworldClimate &mClimate;
    // the frozen ocean builder's own factory, from the level seed cut to 32 bits
    XoroshiroPositionalFactory mIcebergFactory;
    std::vector<SurfaceSimplexNoise> mSurfaceNoise;
    NormalNoisePtr mNoise;
    std::vector<SurfaceSimplexNoise> mAdjustmentNoise;
    std::vector<SurfaceSimplexNoise> mSwampNoise;
    NormalNoisePtr mIcebergPillarNoise;
    NormalNoisePtr mIcebergPillarRoofNoise;
    NormalNoisePtr mIcebergSurfaceNoise;
    NoiseGradientSurfaceBuilder mSulfurCaves;
};
