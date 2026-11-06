#pragma once

#include "World/Level/LevelGen/Biome/BiomeSource.h"
#include "World/Level/LevelGen/End/Noise/TheEndOctaveNoise.h"

#include <array>
#include <cstdint>
#include <random>

// climate picked on a grid of 4 blocks, then spread to single blocks by a jittered nearest point zoom
class VanillaNetherBiomeSource : public BiomeSource {
public:
    explicit VanillaNetherBiomeSource(int64_t seed);

    ~VanillaNetherBiomeSource() override = default;

    int32_t pickBiomeId(int32_t x, int32_t y, int32_t z) const override;

    // fills biomes[(x << 4) | z] for one chunk, sharing the climate grid between its columns
    void pickChunk(int32_t chunkX, int32_t chunkZ, int32_t *biomes) const;

private:
    class ClimateNoise {
    public:
        explicit ClimateNoise(std::mt19937 random);

        float getValue(int32_t gridX, int32_t gridZ) const;

    private:
        TheEndOctaveNoise mFirst;
        TheEndOctaveNoise mSecond;

        ClimateNoise(std::mt19937 &random, int32_t octaves);
    };

    int32_t _pickClimateBiome(int32_t gridX, int32_t gridZ) const;

    void _jitter(int32_t gridX, int32_t gridZ, float &x, float &z) const;

    // index of the nearest of the four corners of a cell, for the block at (localX, localZ) of that cell
    static int32_t _nearestCorner(const float *cornerX, const float *cornerZ, int32_t localX, int32_t localZ);

    std::array<ClimateNoise, 4> mNoises;
    uint64_t mZoomSeed;
};
