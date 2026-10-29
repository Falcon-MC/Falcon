#include "World/Level/LevelGen/Overworld/Feature/Ore/GoldOreMesaFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &goldOreState() {
        static const BlockState state = VanillaBlocks::GOLD_ORE().toBlockState();
        return state;
    }

    const BlockState &deepslateGoldOreState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE_GOLD_ORE().toBlockState();
        return state;
    }
}

const BlockState &GoldOreMesaFeature::getState(const BlockState &original) const {
    if (original.mName == "minecraft:stone")
        return goldOreState();

    if (original.mName == "minecraft:deepslate")
        return deepslateGoldOreState();

    return original;
}

int32_t GoldOreMesaFeature::getClusterCount() const {
    return 50;
}

int32_t GoldOreMesaFeature::getClusterSize() const {
    return 9;
}

int32_t GoldOreMesaFeature::getMinHeight() const {
    return 32;
}

int32_t GoldOreMesaFeature::getMaxHeight() const {
    return 256;
}

const char *GoldOreMesaFeature::name() const {
    return "minecraft:mesa_underground_gold_ore_feature";
}
