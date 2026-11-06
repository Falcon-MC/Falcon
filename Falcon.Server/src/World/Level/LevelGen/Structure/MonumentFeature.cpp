#include "World/Level/LevelGen/Structure/MonumentFeature.h"

#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

namespace {
    const int32_t SPACING = 32;
    const int32_t SEPARATION = 5;
    const int32_t SALT = 10387313;

    const int32_t SEA_LEVEL = 63;
    // biomes are sampled once per cell: deep ocean around the centre, then any water further out
    const int32_t DEEP_RADIUS = 16;
    const int32_t WATER_RADIUS = 29;
    const int32_t CELL_SIZE = 4;

    const MonumentFeature gMonument;

    bool isWaterBiome(int32_t biomeId) {
        return BiomeChunkGenDataRegistry::hasTag(biomeId, "ocean") || BiomeChunkGenDataRegistry::hasTag(biomeId, "river");
    }
}

MonumentFeature::MonumentFeature() : StructureFeature("monument", SPACING, SEPARATION, SALT, Spread::Triangular) {}

bool MonumentFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t blockX = chunkX * 16 + 8;
    const int32_t blockZ = chunkZ * 16 + 8;
    for (int32_t dx = -DEEP_RADIUS; dx <= DEEP_RADIUS; dx += CELL_SIZE) {
        for (int32_t dz = -DEEP_RADIUS; dz <= DEEP_RADIUS; dz += CELL_SIZE) {
            if (!BiomeChunkGenDataRegistry::hasTag(generator.pickBiome(blockX + dx, SEA_LEVEL, blockZ + dz), "deep"))
                return false;
        }
    }

    for (int32_t dx = -WATER_RADIUS; dx <= WATER_RADIUS; dx += CELL_SIZE) {
        for (int32_t dz = -WATER_RADIUS; dz <= WATER_RADIUS; dz += CELL_SIZE) {
            if (!isWaterBiome(generator.pickBiome(blockX + dx, SEA_LEVEL, blockZ + dz)))
                return false;
        }
    }

    return true;
}
