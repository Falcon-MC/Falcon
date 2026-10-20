#include "World/Level/LevelGen/Nether/Feature/Ore/AncientDebrisSmallFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

const BlockState &AncientDebrisSmallFeature::getState(const BlockState &original) const {
    (void) original;

    static const BlockState state = VanillaBlocks::ANCIENT_DEBRIS().toBlockState();
    return state;
}

int32_t AncientDebrisSmallFeature::getClusterCount() const {
    return 3;
}

int32_t AncientDebrisSmallFeature::getClusterSize() const {
    return 2;
}

int32_t AncientDebrisSmallFeature::getMinHeight() const {
    return 8;
}

int32_t AncientDebrisSmallFeature::getMaxHeight() const {
    return 119;
}

float AncientDebrisSmallFeature::getSkipAir() const {
    return 1.0f;
}

const char *AncientDebrisSmallFeature::name() const {
    return "nether_ancientdebris_small";
}
