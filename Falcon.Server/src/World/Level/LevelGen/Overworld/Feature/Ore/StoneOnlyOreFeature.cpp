#include "World/Level/LevelGen/Overworld/Feature/Ore/StoneOnlyOreFeature.h"

bool StoneOnlyOreFeature::canBeReplaced(const BlockState &state) const {
    return state == stoneState();
}
