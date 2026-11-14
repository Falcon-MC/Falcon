#include "World/Level/LevelGen/Structure/NetherFortressFeature.h"

#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"

namespace {
    const int32_t SPACING = 30;
    const int32_t SEPARATION = 4;
    const int32_t SALT = 30084232;

    // nextInt(6) below 2 is a fortress, the other draws a bastion
    const int32_t COMPLEX_ROLL = 6;
    const int32_t FORTRESS_ROLLS = 2;

    const NetherFortressFeature gNetherFortress;
}

NetherFortressFeature::NetherFortressFeature()
        : StructureFeature("fortress", SPACING, SEPARATION, SALT, Spread::Linear) {}

std::mt19937 NetherFortressFeature::createFortressRandom(int64_t seed, int32_t chunkX, int32_t chunkZ) const {
    std::mt19937 random = createStartRandom(seed, chunkX, chunkZ);
    FeatureRandom::nextInt(random, COMPLEX_ROLL);
    return random;
}

bool NetherFortressFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    // the biome list holds all five nether biomes in a current world, so the biome test passes anywhere there
    if (generator.getDimensionType() != DimensionType::Nether)
        return false;

    std::mt19937 random = createStartRandom(generator.getSeed(), chunkX, chunkZ);
    return FeatureRandom::nextInt(random, COMPLEX_ROLL) < FORTRESS_ROLLS;
}
