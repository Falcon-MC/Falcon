#include "World/Level/LevelGen/Overworld/Feature/Ore/CopperOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &copperOreState() {
        static const BlockState state = VanillaBlocks::COPPER_ORE().toBlockState();
        return state;
    }

    const BlockState &deepslateCopperOreState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE_COPPER_ORE().toBlockState();
        return state;
    }
}

const BlockState &CopperOreFeature::getState(const BlockState &original) const {
    if (original.mName == "minecraft:stone")
        return copperOreState();

    if (original.mName == "minecraft:deepslate")
        return deepslateCopperOreState();

    return original;
}

int32_t CopperOreFeature::getClusterCount() const {
    return 16;
}

int32_t CopperOreFeature::getClusterSize() const {
    return 10;
}

int32_t CopperOreFeature::getMinHeight() const {
    return -16;
}

int32_t CopperOreFeature::getMaxHeight() const {
    return 112;
}

OreFeature::ConcentrationType CopperOreFeature::getConcentration() const {
    return ConcentrationType::TRIANGLE;
}

const char *CopperOreFeature::name() const {
    return "minecraft:overworld_underground_copper_ore_feature";
}
