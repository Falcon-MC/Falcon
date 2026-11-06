#include "World/Level/LevelGen/Structure/VillageStructure.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <string>

namespace {
    const int32_t MAX_DEPTH = 6;
    const int32_t SEA_LEVEL = 63;
    const float ABANDONED_CHANCE = 0.02f;

    const char *styleOf(int32_t biomeId) {
        switch (biomeId) {
            case BiomeIds::DESERT:
                return "desert";
            case BiomeIds::ICE_PLAINS:
            case BiomeIds::COLD_TAIGA:
                return "snowy";
            case BiomeIds::SAVANNA:
                return "savanna";
            case BiomeIds::TAIGA:
                return "taiga";
            default:
                return "plains";
        }
    }
}

std::vector<JigsawPiece> VillageStructure::plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ) {
    const int32_t cornerX = startX * 16;
    const int32_t cornerZ = startZ * 16;
    const int32_t surface = generator.getPreliminarySurfaceLevel(startX * 4, startZ * 4);
    const std::string style = styleOf(generator.pickBiome(cornerX + 8, surface, cornerZ + 8));

    const StructureFeature *village = StructureFeature::get("village");
    if (village == nullptr)
        return {};

    std::mt19937 random = village->createStartRandom(generator.getSeed(), startX, startZ);
    const int32_t quarterTurns = FeatureRandom::nextInt(random, 4);
    const bool abandoned = FeatureRandom::nextFloat(random) < ABANDONED_CHANCE;

    const JigsawPools::Pool *centres =
            JigsawPools::get("minecraft:village/" + std::string(abandoned ? "abandoned/" : "") + style + "/town_centers");
    if (centres == nullptr || centres->mSlots.empty())
        return {};

    const JigsawElementData *start = centres->mSlots[(size_t) FeatureRandom::nextInt(random, (int32_t) centres->mSlots.size())];
    if (start == nullptr)
        return {};

    JigsawAssembler assembler(generator, random, MAX_DEPTH, SEA_LEVEL);
    return assembler.assemble(*start, cornerX, surface, cornerZ, quarterTurns);
}
