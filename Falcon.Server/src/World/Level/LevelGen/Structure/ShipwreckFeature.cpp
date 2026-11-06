#include "World/Level/LevelGen/Structure/ShipwreckFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

#include <algorithm>
#include <array>

namespace {
    const int32_t SPACING = 24;
    const int32_t SEPARATION = 4;
    const int32_t SALT = 165745295;

    const int32_t BEACH_RADIUS = 10;
    const int32_t OCEAN_RADIUS = 20;
    const int32_t MONUMENT_DISTANCE = 5;

    const ShipwreckFeature gShipwreck;

    bool isShipwreckBiome(int32_t biomeId) {
        static const std::array<int32_t, 12> biomes = {
                BiomeIds::BEACH, BiomeIds::COLD_BEACH, BiomeIds::MUSHROOM_ISLAND_SHORE, BiomeIds::OCEAN,
                BiomeIds::DEEP_OCEAN, BiomeIds::COLD_OCEAN, BiomeIds::DEEP_COLD_OCEAN, BiomeIds::LUKEWARM_OCEAN,
                BiomeIds::DEEP_LUKEWARM_OCEAN, BiomeIds::FROZEN_OCEAN, BiomeIds::DEEP_FROZEN_OCEAN,
                BiomeIds::WARM_OCEAN};
        return std::find(biomes.begin(), biomes.end(), biomeId) != biomes.end();
    }
}

ShipwreckFeature::ShipwreckFeature() : StructureFeature("shipwreck", SPACING, SEPARATION, SALT, Spread::Linear) {}

bool ShipwreckFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t blockX = chunkX * 16 + 8;
    const int32_t blockZ = chunkZ * 16 + 8;
    const int32_t height = generator.getPreliminarySurfaceLevel(chunkX * 4, chunkZ * 4);
    const int32_t biome = generator.pickBiome(blockX, height, blockZ);
    if (!isShipwreckBiome(biome))
        return false;

    const int32_t radius = isBeachBiome(biome) ? BEACH_RADIUS : OCEAN_RADIUS;
    if (!_hasOnlyBiomes(generator, blockX, height, blockZ, radius, [biome](int32_t other) { return other == biome; }))
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

bool ShipwreckFeature::isBeachBiome(int32_t biomeId) {
    return biomeId == BiomeIds::MUSHROOM_ISLAND_SHORE || biomeId == BiomeIds::BEACH
           || biomeId == BiomeIds::COLD_BEACH;
}
