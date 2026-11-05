#include "World/Level/LevelGen/Structure/TempleFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

namespace {
    const int32_t SPACING = 32;
    const int32_t SEPARATION = 8;
    const int32_t SALT = 14357617;

    const TempleFeature gTemple;
}

TempleFeature::TempleFeature() : StructureFeature("temple", SPACING, SEPARATION, SALT, Spread::Linear) {}

bool TempleFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t blockX = chunkX * 16 + 8;
    const int32_t blockZ = chunkZ * 16 + 8;
    switch (generator.pickBiome(blockX, generator.getPreliminaryHeight(blockX, blockZ), blockZ)) {
        case BiomeIds::DESERT:
        case BiomeIds::JUNGLE:
        case BiomeIds::SWAMPLAND:
        case BiomeIds::COLD_TAIGA:
        case BiomeIds::ICE_PLAINS:
            return true;
        default:
            return false;
    }
}
