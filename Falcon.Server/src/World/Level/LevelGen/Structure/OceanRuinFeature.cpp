#include "World/Level/LevelGen/Structure/OceanRuinFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

#include <algorithm>
#include <array>

namespace {
    //worlds older than 1.18 use 12 and 7
    const int32_t SPACING = 20;
    const int32_t SEPARATION = 8;
    const int32_t SALT = 14357621;

    const int32_t MONUMENT_DISTANCE = 5;

    const OceanRuinFeature gOceanRuin;

    bool isRuinBiome(int32_t biomeId) {
        static const std::array<int32_t, 10> biomes = {BiomeIds::OCEAN,          BiomeIds::DEEP_OCEAN,
                                                       BiomeIds::COLD_OCEAN,     BiomeIds::DEEP_COLD_OCEAN,
                                                       BiomeIds::FROZEN_OCEAN,   BiomeIds::DEEP_FROZEN_OCEAN,
                                                       BiomeIds::LUKEWARM_OCEAN, BiomeIds::DEEP_LUKEWARM_OCEAN,
                                                       BiomeIds::WARM_OCEAN,     BiomeIds::DEEP_WARM_OCEAN};
        return std::find(biomes.begin(), biomes.end(), biomeId) != biomes.end();
    }
}

OceanRuinFeature::OceanRuinFeature() : StructureFeature("ocean_ruin", SPACING, SEPARATION, SALT, Spread::Linear) {}

bool OceanRuinFeature::getConfig(int32_t biomeId, Config &out) {
    // written by the vanilla biome registration
    switch (biomeId) {
        case BiomeIds::OCEAN:
        case BiomeIds::LEGACY_FROZEN_OCEAN:
        case BiomeIds::COLD_OCEAN:
        case BiomeIds::FROZEN_OCEAN:
            out = {false, 0.3f, 0.25f};
            return true;
        case BiomeIds::DEEP_OCEAN:
        case BiomeIds::DEEP_COLD_OCEAN:
        case BiomeIds::DEEP_FROZEN_OCEAN:
            out = {false, 0.5f, 0.4f};
            return true;
        case BiomeIds::WARM_OCEAN:
        case BiomeIds::DEEP_WARM_OCEAN:
        case BiomeIds::LUKEWARM_OCEAN:
        case BiomeIds::DEEP_LUKEWARM_OCEAN:
            out = {true, 0.3f, 0.5f};
            return true;
        default:
            return false;
    }
}

bool OceanRuinFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t height = generator.getPreliminarySurfaceLevel(chunkX * 4, chunkZ * 4);
    if (!isRuinBiome(generator.pickBiome(chunkX * 16 + 8, height, chunkZ * 16 + 8)))
        return false;

    const StructureFeature *monument = StructureFeature::get("monument");
    for (int32_t x = chunkX - MONUMENT_DISTANCE; x < chunkX + MONUMENT_DISTANCE; ++x) {
        for (int32_t z = chunkZ - MONUMENT_DISTANCE; z < chunkZ + MONUMENT_DISTANCE; ++z) {
            if (monument->isStartChunk(generator, x, z))
                return false;
        }
    }

    return true;
}
