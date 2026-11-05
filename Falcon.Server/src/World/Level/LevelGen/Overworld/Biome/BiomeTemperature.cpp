#include "World/Level/LevelGen/Overworld/Biome/BiomeTemperature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Overworld/Biome/ClimateAttributes.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceSimplexNoise.h"

#include <random>
#include <vector>

namespace {
    // Dimension sea level (dimension +204)
    constexpr int32_t SEA_LEVEL = 63;
    // 17 for worlds of 1.18 and later, 1 before (version 1.18.0 at qword_1528D9DD0)
    constexpr int32_t COOLING_START = SEA_LEVEL + 17;
    // the column noise is scaled by 8, 4 before 1.18 (sub_145D3C5D0)
    constexpr float COLUMN_NOISE_SCALE = 8.0f;

    // octaves seeded by the game's Mersenne twister Random, each sampled at half the frequency of the last and
    // weighted by the inverse (sub_145C2D720)
    class OctaveNoise {
    public:
        OctaveNoise(uint32_t seed, int32_t octaves) {
            std::mt19937 random(seed);
            for (int32_t i = 0; i < octaves; i++)
                mOctaves.emplace_back(random);
        }

        float getValue(float x, float z) const {
            float value = 0.0f;
            float scale = 1.0f;
            for (const SurfaceSimplexNoise &octave: mOctaves) {
                value = value + octave.getValue(scale * x, scale * z) / scale;
                scale = scale * 0.5f;
            }
            return value;
        }

    private:
        std::vector<SurfaceSimplexNoise> mOctaves;
    };

    // minecraft:partially_frozen, on frozen_ocean and deep_frozen_ocean in the BDS 1.26.61 biomes
    bool partiallyFrozen(int32_t biomeId) {
        return biomeId == BiomeIds::FROZEN_OCEAN || biomeId == BiomeIds::DEEP_FROZEN_OCEAN;
    }

    // the chunk keeps it per column as a signed byte in eighths (sub_145D3C5D0)
    float columnNoise(int32_t x, int32_t z) {
        static const OctaveNoise noise(1234, 1);
        const float value = noise.getValue(static_cast<float>(x) * 0.125f, static_cast<float>(z) * 0.125f);
        const auto stored = static_cast<int8_t>(static_cast<int32_t>((value * COLUMN_NOISE_SCALE) * 8.0f));
        return static_cast<float>(stored) * 0.125f;
    }
}

float BiomeTemperature::at(int32_t biomeId, int32_t x, int32_t y, int32_t z) {
    const ClimateAttributes *climate = ClimateAttributes::getForBiome(biomeId);
    float temperature = climate != nullptr ? climate->mTemperature : 0.5f;

    if (partiallyFrozen(biomeId)) {
        static const OctaveNoise patches(3456, 3);
        static const OctaveNoise edges(2345, 1);
        const float fx = static_cast<float>(x);
        const float fz = static_cast<float>(z);
        if (patches.getValue(fx * 0.05f, fz * 0.05f) + edges.getValue(fx * 0.2f, fz * 0.2f) < 0.3f
            && edges.getValue(fx * 0.09f, fz * 0.09f) < 0.9f)
            temperature = 0.2f;
    }

    if (y > COOLING_START) {
        const float above = (static_cast<float>(y) + columnNoise(x, z)) - static_cast<float>(COOLING_START);
        temperature = temperature + (above * -0.05f) / 40.0f;
    }
    return temperature;
}
