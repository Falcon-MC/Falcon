#include "World/Level/LevelGen/Overworld/Feature/Ore/GoldOreLowerFeature.h"

int32_t GoldOreLowerFeature::getClusterCount() const {
    return 2;
}

bool GoldOreLowerFeature::isRare() const {
    return true;
}

int32_t GoldOreLowerFeature::getMinHeight() const {
    return -64;
}

int32_t GoldOreLowerFeature::getMaxHeight() const {
    return -48;
}

float GoldOreLowerFeature::getSkipAir() const {
    return 0.5f;
}

const char *GoldOreLowerFeature::name() const {
    return "minecraft:overworld_underground_gold_ore_lower_feature";
}
