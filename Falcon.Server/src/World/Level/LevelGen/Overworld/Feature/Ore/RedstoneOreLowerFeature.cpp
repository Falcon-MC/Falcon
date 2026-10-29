#include "World/Level/LevelGen/Overworld/Feature/Ore/RedstoneOreLowerFeature.h"

int32_t RedstoneOreLowerFeature::getClusterCount() const {
    return 8;
}

int32_t RedstoneOreLowerFeature::getMaxHeight() const {
    return -32;
}

OreFeature::ConcentrationType RedstoneOreLowerFeature::getConcentration() const {
    return ConcentrationType::TRIANGLE;
}

const char *RedstoneOreLowerFeature::name() const {
    return "minecraft:overworld_underground_redstone_ore_lower_feature";
}
