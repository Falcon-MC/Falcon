#include "World/Level/LevelGen/Overworld/Feature/Ore/GravelOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &gravelState() {
        static const BlockState state = VanillaBlocks::GRAVEL().toBlockState();
        return state;
    }
}

const BlockState &GravelOreFeature::getState(const BlockState &original) const {
    (void) original;
    return gravelState();
}

int32_t GravelOreFeature::getClusterCount() const {
    return 14;
}

int32_t GravelOreFeature::getClusterSize() const {
    return 33;
}

int32_t GravelOreFeature::getMinHeight() const {
    return -64;
}

int32_t GravelOreFeature::getMaxHeight() const {
    return 320;
}

const char *GravelOreFeature::name() const {
    return "minecraft:overworld_underground_gravel_ore_feature";
}
