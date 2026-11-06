#include "World/Level/LevelGen/Structure/AncientCityFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

namespace {
    const int32_t SPACING = 24;
    const int32_t SEPARATION = 8;
    const int32_t SALT = 20083232;

    const int32_t CITY_Y = -27;

    const AncientCityFeature gAncientCity;
}

AncientCityFeature::AncientCityFeature()
        : StructureFeature("ancient_city", SPACING, SEPARATION, SALT, Spread::Triangular) {}

bool AncientCityFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    return generator.pickBiome(chunkX * 16 + 8, CITY_Y, chunkZ * 16 + 8) == BiomeIds::DEEP_DARK;
}
