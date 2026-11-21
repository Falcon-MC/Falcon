#include "World/Level/LevelGen/Overworld/Feature/Multi/OverworldCaveCarverAggregateFeature.h"

const char *OverworldCaveCarverAggregateFeature::name() const {
    return "minecraft:overworld_cave_carver_feature";
}

std::vector<const char *> OverworldCaveCarverAggregateFeature::getFeatures() const {
    // the carvers themselves run in OverworldGenerator::generate, like the game's pregeneration_pass
    return {"minecraft:monster_room"};
}
