#include "World/Level/LevelGen/Overworld/Feature/Ore/CoalOreLowerFeature.h"

int32_t CoalOreLowerFeature::getClusterCount() const {
    return 20;
}

int32_t CoalOreLowerFeature::getMinHeight() const {
    return 0;
}

int32_t CoalOreLowerFeature::getMaxHeight() const {
    return 192;
}

OreFeature::ConcentrationType CoalOreLowerFeature::getConcentration() const {
    return ConcentrationType::TRIANGLE;
}

float CoalOreLowerFeature::getSkipAir() const {
    return 0.5f;
}

const char *CoalOreLowerFeature::name() const {
    return "minecraft:overworld_underground_coal_ore_lower_feature";
}
