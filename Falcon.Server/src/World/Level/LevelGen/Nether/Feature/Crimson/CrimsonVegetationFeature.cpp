#include "World/Level/LevelGen/Nether/Feature/Crimson/CrimsonVegetationFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"

int32_t CrimsonVegetationFeature::getBiomeId() const {
    return NetherBiomeIds::CRIMSON_FOREST;
}

const char *CrimsonVegetationFeature::getNyliumIdentifier() const {
    return "minecraft:crimson_nylium";
}

const BlockState &CrimsonVegetationFeature::getFungusState() const {
    static const BlockState state = VanillaBlocks::CRIMSON_FUNGUS().toBlockState();
    return state;
}

const BlockState &CrimsonVegetationFeature::getRootsState() const {
    static const BlockState state = VanillaBlocks::CRIMSON_ROOTS().toBlockState();
    return state;
}

const char *CrimsonVegetationFeature::name() const {
    return "nether_crimson_grasses";
}
