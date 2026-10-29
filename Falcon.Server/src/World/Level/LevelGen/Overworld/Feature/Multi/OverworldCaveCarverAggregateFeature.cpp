#include "World/Level/LevelGen/Overworld/Feature/Multi/OverworldCaveCarverAggregateFeature.h"

const char *OverworldCaveCarverAggregateFeature::name() const {
    return "minecraft:overworld_cave_carver_feature";
}

std::vector<const char *> OverworldCaveCarverAggregateFeature::getFeatures() const {
    return {
            "minecraft:overworld_cave",
            "minecraft:cave_extra_underground",
            "minecraft:canyon_carver",
            "minecraft:monster_room"
    };
}
