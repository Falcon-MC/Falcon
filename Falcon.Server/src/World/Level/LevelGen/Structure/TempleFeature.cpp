#include "World/Level/LevelGen/Structure/TempleFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

namespace {
    const int32_t SPACING = 32;
    const int32_t SEPARATION = 8;
    const int32_t SALT = 14357617;

    const int32_t SEA_LEVEL = 63;

    const TempleFeature gTemple;

    bool isTempleBiome(int32_t biomeId) {
        switch (biomeId) {
            case BiomeIds::DESERT:
            case BiomeIds::DESERT_HILLS:
            case BiomeIds::JUNGLE:
            case BiomeIds::JUNGLE_HILLS:
            case BiomeIds::SWAMPLAND:
            case BiomeIds::SWAMPLAND_MUTATED:
            case BiomeIds::COLD_TAIGA:
            case BiomeIds::SNOWY_SLOPES:
            case BiomeIds::ICE_PLAINS:
                return true;
            default:
                return false;
        }
    }
}

TempleFeature::TempleFeature() : StructureFeature("temple", SPACING, SEPARATION, SALT, Spread::Linear) {}

bool TempleFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t height = generator.getPreliminarySurfaceLevel(chunkX * 4, chunkZ * 4);
    return isTempleBiome(generator.pickBiome(chunkX * 16 + 8, height, chunkZ * 16 + 8))
           && generator.getPreliminaryLowestHeight(chunkX, chunkZ) >= SEA_LEVEL;
}
