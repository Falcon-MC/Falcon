#include "World/Level/LevelGen/Structure/MonumentFeature.h"

#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

namespace {
    const int32_t SPACING = 32;
    const int32_t SEPARATION = 5;
    const int32_t SALT = 10387313;

    // deep ocean around the centre, then any water further out
    const int32_t DEEP_RADIUS = 16;
    const int32_t WATER_RADIUS = 29;

    const MonumentFeature gMonument;

    bool isDeepBiome(int32_t biomeId) {
        return BiomeChunkGenDataRegistry::hasTag(biomeId, "deep");
    }

    bool isWaterBiome(int32_t biomeId) {
        return BiomeChunkGenDataRegistry::hasTag(biomeId, "ocean") || BiomeChunkGenDataRegistry::hasTag(biomeId, "river");
    }
}

MonumentFeature::MonumentFeature() : StructureFeature("monument", SPACING, SEPARATION, SALT, Spread::Triangular) {}

bool MonumentFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t blockX = chunkX * 16 + 8;
    const int32_t blockZ = chunkZ * 16 + 8;
    const int32_t height = generator.getPreliminarySurfaceLevel(chunkX * 4, chunkZ * 4);
    return _hasOnlyBiomes(generator, blockX, height, blockZ, DEEP_RADIUS, isDeepBiome)
           && _hasOnlyBiomes(generator, blockX, height, blockZ, WATER_RADIUS, isWaterBiome);
}
