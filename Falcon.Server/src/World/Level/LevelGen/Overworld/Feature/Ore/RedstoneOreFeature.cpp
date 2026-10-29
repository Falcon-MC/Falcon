#include "World/Level/LevelGen/Overworld/Feature/Ore/RedstoneOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &redstoneOreState() {
        static const BlockState state = VanillaBlocks::REDSTONE_ORE().toBlockState();
        return state;
    }

    const BlockState &deepslateRedstoneOreState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE_REDSTONE_ORE().toBlockState();
        return state;
    }
}

const BlockState &RedstoneOreFeature::getState(const BlockState &original) const {
    if (original.mName == "minecraft:stone")
        return redstoneOreState();

    if (original.mName == "minecraft:deepslate")
        return deepslateRedstoneOreState();

    return original;
}

int32_t RedstoneOreFeature::getClusterCount() const {
    return 4;
}

int32_t RedstoneOreFeature::getClusterSize() const {
    return 8;
}

int32_t RedstoneOreFeature::getMinHeight() const {
    return -64;
}

int32_t RedstoneOreFeature::getMaxHeight() const {
    return 15;
}

const char *RedstoneOreFeature::name() const {
    return "minecraft:overworld_underground_redstone_ore_feature";
}
