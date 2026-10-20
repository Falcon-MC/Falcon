#include "World/Level/LevelGen/Overworld/Feature/Ore/DiamondOreLargeFeature.h"

int32_t DiamondOreLargeFeature::getClusterCount() const {
    return 9;
}

int32_t DiamondOreLargeFeature::getClusterSize() const {
    return 12;
}

float DiamondOreLargeFeature::getSkipAir() const {
    return 0.7f;
}

bool DiamondOreLargeFeature::isRare() const {
    return true;
}

const char *DiamondOreLargeFeature::name() const {
    return "minecraft:overworld_underground_diamond_ore_large_feature";
}
