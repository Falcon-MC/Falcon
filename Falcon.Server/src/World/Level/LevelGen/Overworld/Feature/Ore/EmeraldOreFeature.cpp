#include "World/Level/LevelGen/Overworld/Feature/Ore/EmeraldOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &emeraldOreState() {
        static const BlockState state = VanillaBlocks::EMERALD_ORE().toBlockState();
        return state;
    }

    const BlockState &deepslateEmeraldOreState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE_EMERALD_ORE().toBlockState();
        return state;
    }
}

const BlockState &EmeraldOreFeature::getState(const BlockState &original) const {
    if (original.mName == "minecraft:stone")
        return emeraldOreState();

    if (original.mName == "minecraft:deepslate")
        return deepslateEmeraldOreState();

    return original;
}

int32_t EmeraldOreFeature::getClusterCount() const {
    return 100;
}

int32_t EmeraldOreFeature::getClusterSize() const {
    return 3;
}

int32_t EmeraldOreFeature::getMinHeight() const {
    return -16;
}

int32_t EmeraldOreFeature::getMaxHeight() const {
    return 420;
}

const char *EmeraldOreFeature::name() const {
    return "minecraft:overworld_underground_emerald_ore_feature";
}
