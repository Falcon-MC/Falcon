#include "World/Level/LevelGen/Structure/VillageFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

namespace {
    const int32_t SPACING = 34;
    const int32_t SEPARATION = 8;
    const int32_t SALT = 10387312;

    const VillageFeature gVillage;
}

VillageFeature::VillageFeature() : StructureFeature("village", SPACING, SEPARATION, SALT, Spread::Triangular) {}

bool VillageFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t blockX = chunkX * 16 + 8;
    const int32_t blockZ = chunkZ * 16 + 8;
    // the biome is read at the surface: a candidate over a deep dark or dripstone cave still counts as plains
    switch (generator.pickBiome(blockX, generator.getPreliminaryHeight(blockX, blockZ), blockZ)) {
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
