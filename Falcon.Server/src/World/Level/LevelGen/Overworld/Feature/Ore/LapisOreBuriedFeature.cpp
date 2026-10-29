#include "World/Level/LevelGen/Overworld/Feature/Ore/LapisOreBuriedFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &lapisOreState() {
        static const BlockState state = VanillaBlocks::LAPIS_ORE().toBlockState();
        return state;
    }

    const BlockState &deepslateLapisOreState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE_LAPIS_ORE().toBlockState();
        return state;
    }
}

const BlockState &LapisOreBuriedFeature::getState(const BlockState &original) const {
    if (original.mName == "minecraft:stone")
        return lapisOreState();

    if (original.mName == "minecraft:deepslate")
        return deepslateLapisOreState();

    return original;
}

int32_t LapisOreBuriedFeature::getClusterCount() const {
    return 4;
}

int32_t LapisOreBuriedFeature::getClusterSize() const {
    return 7;
}

int32_t LapisOreBuriedFeature::getMinHeight() const {
    return -64;
}

int32_t LapisOreBuriedFeature::getMaxHeight() const {
    return 64;
}

float LapisOreBuriedFeature::getSkipAir() const {
    return 1.0f;
}

const char *LapisOreBuriedFeature::name() const {
    return "minecraft:overworld_underground_lapis_ore_buried_feature";
}
