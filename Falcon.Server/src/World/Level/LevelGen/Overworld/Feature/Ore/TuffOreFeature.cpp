#include "World/Level/LevelGen/Overworld/Feature/Ore/TuffOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &tuffState() {
        static const BlockState state = VanillaBlocks::TUFF().toBlockState();
        return state;
    }
}

const BlockState &TuffOreFeature::getState(const BlockState &original) const {
    (void) original;
    return tuffState();
}

int32_t TuffOreFeature::getClusterCount() const {
    return 2;
}

int32_t TuffOreFeature::getClusterSize() const {
    return 64;
}

int32_t TuffOreFeature::getMinHeight() const {
    return -64;
}

int32_t TuffOreFeature::getMaxHeight() const {
    return 0;
}

const char *TuffOreFeature::name() const {
    return "minecraft:overworld_underground_tuff_feature";
}
