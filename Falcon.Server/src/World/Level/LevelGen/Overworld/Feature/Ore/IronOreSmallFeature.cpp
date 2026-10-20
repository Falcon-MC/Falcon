#include "World/Level/LevelGen/Overworld/Feature/Ore/IronOreSmallFeature.h"

int32_t IronOreSmallFeature::getClusterSize() const {
    return 4;
}

int32_t IronOreSmallFeature::getMinHeight() const {
    return -64;
}

int32_t IronOreSmallFeature::getMaxHeight() const {
    return 72;
}

OreFeature::ConcentrationType IronOreSmallFeature::getConcentration() const {
    return ConcentrationType::UNIFORM;
}

const char *IronOreSmallFeature::name() const {
    return "minecraft:overworld_underground_iron_ore_small_feature";
}
