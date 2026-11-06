#include "World/Level/LevelGen/Structure/VillageFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

namespace {
    const int32_t SPACING = 34;
    const int32_t SEPARATION = 8;
    const int32_t SALT = 10387312;

    const int32_t BIOME_RADIUS = 2;

    const VillageFeature gVillage;

    bool isVillageBiome(int32_t biomeId) {
        switch (biomeId) {
            case BiomeIds::PLAINS:
            case BiomeIds::SUNFLOWER_PLAINS:
            case BiomeIds::DESERT:
            case BiomeIds::SAVANNA:
            case BiomeIds::TAIGA:
            case BiomeIds::COLD_TAIGA:
            case BiomeIds::ICE_PLAINS:
            case BiomeIds::MEADOW:
                return true;
            default:
                return false;
        }
    }
}

VillageFeature::VillageFeature() : StructureFeature("village", SPACING, SEPARATION, SALT, Spread::Triangular) {}

bool VillageFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    // the height comes from the chunk corner while the biomes are read around its centre
    const int32_t height = generator.getPreliminarySurfaceLevel(chunkX * 4, chunkZ * 4);
    return _hasOnlyBiomes(generator, chunkX * 16 + 8, height, chunkZ * 16 + 8, BIOME_RADIUS, isVillageBiome);
}
