#include "World/Level/LevelGen/Overworld/Feature/Ore/CoalOreUpperFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &coalOreState() {
        static const BlockState state = VanillaBlocks::COAL_ORE().toBlockState();
        return state;
    }

    const BlockState &deepslateCoalOreState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE_COAL_ORE().toBlockState();
        return state;
    }
}

const BlockState &CoalOreUpperFeature::getState(const BlockState &original) const {
    if (original.mName == "minecraft:stone")
        return coalOreState();

    if (original.mName == "minecraft:deepslate")
        return deepslateCoalOreState();

    return original;
}

int32_t CoalOreUpperFeature::getClusterCount() const {
    return 30;
}

int32_t CoalOreUpperFeature::getClusterSize() const {
    return 17;
}

int32_t CoalOreUpperFeature::getMinHeight() const {
    return 136;
}

int32_t CoalOreUpperFeature::getMaxHeight() const {
    return 320;
}

const char *CoalOreUpperFeature::name() const {
    return "minecraft:overworld_underground_coal_ore_upper_feature";
}
