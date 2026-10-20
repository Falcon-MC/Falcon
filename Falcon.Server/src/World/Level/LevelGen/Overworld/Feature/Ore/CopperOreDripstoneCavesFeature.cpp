#include "World/Level/LevelGen/Overworld/Feature/Ore/CopperOreDripstoneCavesFeature.h"

int32_t CopperOreDripstoneCavesFeature::getClusterSize() const {
    return 20;
}

const char *CopperOreDripstoneCavesFeature::name() const {
    return "minecraft:dripstone_caves_copper_ore_feature";
}
