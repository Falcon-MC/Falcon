#include "World/Level/LevelGen/Overworld/Feature/Ore/DioriteOreUpperFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &dioriteState() {
        static const BlockState state = VanillaBlocks::DIORITE().toBlockState();
        return state;
    }
}

const BlockState &DioriteOreUpperFeature::getState(const BlockState &original) const {
    (void) original;
    return dioriteState();
}

const char *DioriteOreUpperFeature::name() const {
    return "minecraft:overworld_underground_diorite_upper_feature";
}
