#include "World/Level/LevelGen/Overworld/Feature/Ore/IronOreMiddleFeature.h"

int32_t IronOreMiddleFeature::getClusterCount() const {
    return 10;
}

int32_t IronOreMiddleFeature::getClusterSize() const {
    return 10;
}

int32_t IronOreMiddleFeature::getMinHeight() const {
    return -24;
}

int32_t IronOreMiddleFeature::getMaxHeight() const {
    return 56;
}

const char *IronOreMiddleFeature::name() const {
    return "minecraft:overworld_underground_iron_ore_middle_feature";
}
