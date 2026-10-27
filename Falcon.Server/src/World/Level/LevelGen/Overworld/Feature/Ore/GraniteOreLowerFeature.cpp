#include "World/Level/LevelGen/Overworld/Feature/Ore/GraniteOreLowerFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &graniteState() {
        static const BlockState state = VanillaBlocks::GRANITE().toBlockState();
        return state;
    }
}

const BlockState &GraniteOreLowerFeature::getState(const BlockState &original) const {
    (void) original;
    return graniteState();
}

int32_t GraniteOreLowerFeature::getClusterCount() const {
    return 2;
}

int32_t GraniteOreLowerFeature::getClusterSize() const {
    return 64;
}

int32_t GraniteOreLowerFeature::getMinHeight() const {
    return 0;
}

int32_t GraniteOreLowerFeature::getMaxHeight() const {
    return 60;
}

const char *GraniteOreLowerFeature::name() const {
    return "minecraft:overworld_underground_granite_lower_feature";
}
