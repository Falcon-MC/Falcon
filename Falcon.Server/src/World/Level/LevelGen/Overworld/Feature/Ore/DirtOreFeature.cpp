#include "World/Level/LevelGen/Overworld/Feature/Ore/DirtOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &dirtState() {
        static const BlockState state = VanillaBlocks::DIRT().toBlockState();
        return state;
    }
}

const BlockState &DirtOreFeature::getState(const BlockState &original) const {
    (void) original;
    return dirtState();
}

int32_t DirtOreFeature::getClusterCount() const {
    return 7;
}

int32_t DirtOreFeature::getClusterSize() const {
    return 33;
}

int32_t DirtOreFeature::getMinHeight() const {
    return 0;
}

int32_t DirtOreFeature::getMaxHeight() const {
    return 160;
}

const char *DirtOreFeature::name() const {
    return "minecraft:overworld_underground_dirt_feature";
}
