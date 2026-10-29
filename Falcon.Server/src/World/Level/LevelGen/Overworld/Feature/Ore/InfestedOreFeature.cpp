#include "World/Level/LevelGen/Overworld/Feature/Ore/InfestedOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &infestedStoneState() {
        static const BlockState state = VanillaBlocks::INFESTED_STONE().toBlockState();
        return state;
    }

    const BlockState &infestedDeepslateState() {
        static const BlockState state = VanillaBlocks::INFESTED_DEEPSLATE().toBlockState();
        return state;
    }
}

const BlockState &InfestedOreFeature::getState(const BlockState &original) const {
    if (original.mName == "minecraft:stone")
        return infestedStoneState();

    if (original.mName == "minecraft:deepslate")
        return infestedDeepslateState();

    return original;
}

int32_t InfestedOreFeature::getClusterCount() const {
    return 100;
}

int32_t InfestedOreFeature::getClusterSize() const {
    return 3;
}

int32_t InfestedOreFeature::getMinHeight() const {
    return -16;
}

int32_t InfestedOreFeature::getMaxHeight() const {
    return 420;
}

const char *InfestedOreFeature::name() const {
    return "minecraft:extreme_hills_after_surface_silverfish_feature";
}
