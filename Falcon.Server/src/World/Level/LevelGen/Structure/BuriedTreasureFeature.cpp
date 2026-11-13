#include "World/Level/LevelGen/Structure/BuriedTreasureFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

namespace {
    // ctor sub_14728DDE0, isFeatureChunk sub_14728E0D0 (26.60)
    const int32_t SPACING = 4;
    const int32_t SEPARATION = 2;
    const int32_t SALT = 16842397;

    const int32_t BIOME_RADIUS = 3;

    const BuriedTreasureFeature gBuriedTreasure;

    bool isTreasureBiome(int32_t biomeId) {
        return biomeId == BiomeIds::BEACH || biomeId == BiomeIds::COLD_BEACH || biomeId == BiomeIds::STONE_BEACH
               || biomeId == BiomeIds::MUSHROOM_ISLAND_SHORE;
    }
}

BuriedTreasureFeature::BuriedTreasureFeature()
        : StructureFeature("buried_treasure", SPACING, SEPARATION, SALT, Spread::Triangular) {}

bool BuriedTreasureFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t height = generator.getPreliminarySurfaceLevel(chunkX * 4, chunkZ * 4);
    return _hasOnlyBiomes(generator, chunkX * 16 + 8, height, chunkZ * 16 + 8, BIOME_RADIUS, isTreasureBiome);
}
