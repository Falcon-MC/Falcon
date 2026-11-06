#include "World/Level/LevelGen/Structure/TrialChambersFeature.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

namespace {
    const int32_t SPACING = 34;
    const int32_t SEPARATION = 12;
    const int32_t SALT = 94251327;

    const int32_t CHAMBER_Y = -27;

    const TrialChambersFeature gTrialChambers;
}

TrialChambersFeature::TrialChambersFeature()
        : StructureFeature("trial_chambers", SPACING, SEPARATION, SALT, Spread::Linear, Generator::Congruential) {}

bool TrialChambersFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const int32_t biome = generator.pickBiome(chunkX * 16, CHAMBER_Y, chunkZ * 16);
    return biome != BiomeIds::DEEP_DARK && biome != BiomeIds::SULFUR_CAVES;
}
