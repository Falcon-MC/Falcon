#include "World/Level/LevelGen/Overworld/Feature/Ore/DiamondOreSquareFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &diamondOreState() {
        static const BlockState state = VanillaBlocks::DIAMOND_ORE().toBlockState();
        return state;
    }

    const BlockState &deepslateDiamondOreState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE_DIAMOND_ORE().toBlockState();
        return state;
    }
}

const BlockState &DiamondOreSquareFeature::getState(const BlockState &original) const {
    if (original.mName == "minecraft:stone")
        return diamondOreState();

    if (original.mName == "minecraft:deepslate")
        return deepslateDiamondOreState();

    return original;
}

int32_t DiamondOreSquareFeature::getClusterCount() const {
    return 8;
}

int32_t DiamondOreSquareFeature::getClusterSize() const {
    return 2;
}

int32_t DiamondOreSquareFeature::getMinHeight() const {
    return -64;
}

int32_t DiamondOreSquareFeature::getMaxHeight() const {
    return -4;
}

float DiamondOreSquareFeature::getSkipAir() const {
    return 0.5f;
}

const char *DiamondOreSquareFeature::name() const {
    return "minecraft:overworld_underground_diamond_ore_feature_square";
}
