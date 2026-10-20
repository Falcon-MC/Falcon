#include "World/Level/LevelGen/Overworld/Feature/Ore/DiamondOreFeature.h"

int32_t DiamondOreFeature::getClusterCount() const {
    return 4;
}

int32_t DiamondOreFeature::getClusterSize() const {
    return 7;
}

int32_t DiamondOreFeature::getMaxHeight() const {
    return 16;
}

OreFeature::ConcentrationType DiamondOreFeature::getConcentration() const {
    return ConcentrationType::TRIANGLE;
}

const char *DiamondOreFeature::name() const {
    return "minecraft:overworld_underground_diamond_ore_feature";
}
