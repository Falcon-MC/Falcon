#include "World/Level/LevelGen/Overworld/Feature/Ore/GoldOreFeature.h"

int32_t GoldOreFeature::getClusterCount() const {
    return 4;
}

int32_t GoldOreFeature::getMinHeight() const {
    return -64;
}

int32_t GoldOreFeature::getMaxHeight() const {
    return 32;
}

OreFeature::ConcentrationType GoldOreFeature::getConcentration() const {
    return ConcentrationType::TRIANGLE;
}

float GoldOreFeature::getSkipAir() const {
    return 0.5f;
}

const char *GoldOreFeature::name() const {
    return "minecraft:overworld_underground_gold_ore_feature";
}
