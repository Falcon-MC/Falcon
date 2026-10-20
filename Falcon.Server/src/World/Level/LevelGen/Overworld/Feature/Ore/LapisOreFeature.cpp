#include "World/Level/LevelGen/Overworld/Feature/Ore/LapisOreFeature.h"

int32_t LapisOreFeature::getClusterCount() const {
    return 2;
}

int32_t LapisOreFeature::getMinHeight() const {
    return -32;
}

int32_t LapisOreFeature::getMaxHeight() const {
    return 32;
}

OreFeature::ConcentrationType LapisOreFeature::getConcentration() const {
    return ConcentrationType::TRIANGLE;
}

const char *LapisOreFeature::name() const {
    return "minecraft:overworld_underground_lapis_ore_feature";
}
