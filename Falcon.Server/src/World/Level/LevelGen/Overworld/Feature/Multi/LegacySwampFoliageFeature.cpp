#include "World/Level/LevelGen/Overworld/Feature/Multi/LegacySwampFoliageFeature.h"

const char *LegacySwampFoliageFeature::name() const {
    return "minecraft:legacy:swamp_foliage_feature";
}

std::vector<const char *> LegacySwampFoliageFeature::getFeatures() const {
    return {
            "minecraft:reeds_feature",
            "minecraft:huge_mushroom_feature",
            "minecraft:swamp_oak_tree_feature"
    };
}
