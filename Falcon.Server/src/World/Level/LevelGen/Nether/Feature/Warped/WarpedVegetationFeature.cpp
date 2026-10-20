#include "World/Level/LevelGen/Nether/Feature/Warped/WarpedVegetationFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"

int32_t WarpedVegetationFeature::getBiomeId() const {
    return NetherBiomeIds::WARPED_FOREST;
}

const char *WarpedVegetationFeature::getNyliumIdentifier() const {
    return "minecraft:warped_nylium";
}

const BlockState &WarpedVegetationFeature::getFungusState() const {
    static const BlockState state = VanillaBlocks::WARPED_FUNGUS().toBlockState();
    return state;
}

const BlockState &WarpedVegetationFeature::getRootsState() const {
    static const BlockState state = VanillaBlocks::WARPED_ROOTS().toBlockState();
    return state;
}

const char *WarpedVegetationFeature::name() const {
    return "nether_warped_grasses";
}
