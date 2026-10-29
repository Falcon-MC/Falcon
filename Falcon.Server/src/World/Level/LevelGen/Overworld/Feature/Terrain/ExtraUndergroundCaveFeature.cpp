#include "World/Level/LevelGen/Overworld/Feature/Terrain/ExtraUndergroundCaveFeature.h"

const char *ExtraUndergroundCaveFeature::name() const {
    return "minecraft:cave_extra_underground";
}

float ExtraUndergroundCaveFeature::getCaveProbability() const {
    return 0.07f;
}

int32_t ExtraUndergroundCaveFeature::getCaveMaxY() const {
    return 47;
}
