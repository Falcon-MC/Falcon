#include "World/Level/LevelGen/Overworld/Feature/Multi/RoofedForestAggregateFeature.h"

const char *RoofedForestAggregateFeature::name() const {
    return "minecraft:random_roofed_forest_feature_with_decoration_feature";
}

std::vector<const char *> RoofedForestAggregateFeature::getFeatures() const {
    return {
            "minecraft:legacy:forest_foliage_feature",
            "minecraft:huge_mushroom_feature",
            "minecraft:roofed_forest_tree_feature_rules"
    };
}
