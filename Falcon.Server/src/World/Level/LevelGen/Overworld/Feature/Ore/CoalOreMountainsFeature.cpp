#include "World/Level/LevelGen/Overworld/Feature/Ore/CoalOreMountainsFeature.h"

int32_t CoalOreMountainsFeature::getClusterCount() const {
    return 20;
}

int32_t CoalOreMountainsFeature::getMinHeight() const {
    return 128;
}

int32_t CoalOreMountainsFeature::getMaxHeight() const {
    return 156;
}

const char *CoalOreMountainsFeature::name() const {
    return "minecraft:mountains_underground_coal_ore_feature";
}
