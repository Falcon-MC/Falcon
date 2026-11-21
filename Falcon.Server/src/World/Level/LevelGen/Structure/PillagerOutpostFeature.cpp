#include "World/Level/LevelGen/Structure/PillagerOutpostFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

#include <algorithm>
#include <array>

namespace {
    const int32_t SPACING = 80;
    const int32_t SEPARATION = 24;
    const int32_t SALT = 165745296;

    const PillagerOutpostFeature gPillagerOutpost;

    bool isOutpostBiome(int32_t biomeId) {
        static const std::array<int32_t, 16> biomes = {
                BiomeIds::PLAINS,      BiomeIds::SUNFLOWER_PLAINS, BiomeIds::SAVANNA,      BiomeIds::ICE_PLAINS,
                BiomeIds::TAIGA,       BiomeIds::TAIGA_HILLS,      BiomeIds::COLD_TAIGA,   BiomeIds::COLD_TAIGA_HILLS,
                BiomeIds::DESERT,      BiomeIds::MEADOW,           BiomeIds::FROZEN_PEAKS, BiomeIds::JAGGED_PEAKS,
                BiomeIds::STONY_PEAKS, BiomeIds::SNOWY_SLOPES,     BiomeIds::CHERRY_GROVE, BiomeIds::GROVE};
        return std::find(biomes.begin(), biomes.end(), biomeId) != biomes.end();
    }
}

PillagerOutpostFeature::PillagerOutpostFeature()
        : StructureFeature("pillager_outpost", SPACING, SEPARATION, SALT, Spread::Triangular) {}

bool PillagerOutpostFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t height = generator.getPreliminarySurfaceLevel(chunkX * 4, chunkZ * 4);
    return isOutpostBiome(generator.pickBiome(chunkX * 16 + 8, height, chunkZ * 16 + 8));
}
