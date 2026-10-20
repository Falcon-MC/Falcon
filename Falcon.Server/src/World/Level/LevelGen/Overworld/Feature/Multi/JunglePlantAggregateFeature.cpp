#include "World/Level/LevelGen/Overworld/Feature/Multi/JunglePlantAggregateFeature.h"

const char *JunglePlantAggregateFeature::name() const {
    return "minecraft:scatter_jungle_plant_feature";
}

std::vector<const char *> JunglePlantAggregateFeature::getFeatures() const {
    return {
            "minecraft:jungle_bush",
            "minecraft:jungle_tall_grass_feature",
            "minecraft:grass_double_plant_patch_feature"
    };
}
