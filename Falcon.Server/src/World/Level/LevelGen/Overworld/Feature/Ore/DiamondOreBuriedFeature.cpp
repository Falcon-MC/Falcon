#include "World/Level/LevelGen/Overworld/Feature/Ore/DiamondOreBuriedFeature.h"

int32_t DiamondOreBuriedFeature::getClusterCount() const {
    return 8;
}

int32_t DiamondOreBuriedFeature::getClusterSize() const {
    return 4;
}

float DiamondOreBuriedFeature::getSkipAir() const {
    return 1.0f;
}

const char *DiamondOreBuriedFeature::name() const {
    return "minecraft:overworld_underground_diamond_ore_buried_feature";
}
