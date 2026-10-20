#include "World/Level/LevelGen/Overworld/Feature/Ore/IronOreUpperFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &ironOreState() {
        static const BlockState state = VanillaBlocks::IRON_ORE().toBlockState();
        return state;
    }

    const BlockState &deepslateIronOreState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE_IRON_ORE().toBlockState();
        return state;
    }
}

const BlockState &IronOreUpperFeature::getState(const BlockState &original) const {
    if (original.mName == "minecraft:stone")
        return ironOreState();

    if (original.mName == "minecraft:deepslate")
        return deepslateIronOreState();

    return original;
}

int32_t IronOreUpperFeature::getClusterCount() const {
    return 90;
}

int32_t IronOreUpperFeature::getClusterSize() const {
    return 10;
}

int32_t IronOreUpperFeature::getMinHeight() const {
    return 80;
}

int32_t IronOreUpperFeature::getMaxHeight() const {
    return 384;
}

OreFeature::ConcentrationType IronOreUpperFeature::getConcentration() const {
    return ConcentrationType::TRIANGLE;
}

const char *IronOreUpperFeature::name() const {
    return "minecraft:overworld_underground_iron_ore_upper_feature";
}
