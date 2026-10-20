#include "World/Level/LevelGen/Overworld/Feature/Ore/DioriteOreLowerFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &dioriteState() {
        static const BlockState state = VanillaBlocks::DIORITE().toBlockState();
        return state;
    }
}

const BlockState &DioriteOreLowerFeature::getState(const BlockState &original) const {
    (void) original;
    return dioriteState();
}

const char *DioriteOreLowerFeature::name() const {
    return "minecraft:overworld_underground_diorite_lower_feature";
}
