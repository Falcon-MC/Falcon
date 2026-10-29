#include "World/Level/LevelGen/Nether/Feature/Ore/SoulSandOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

const BlockState &SoulSandOreFeature::getState(const BlockState &original) const {
    (void) original;

    static const BlockState state = VanillaBlocks::SOUL_SAND().toBlockState();
    return state;
}

int32_t SoulSandOreFeature::getClusterCount() const {
    return 12;
}

int32_t SoulSandOreFeature::getClusterSize() const {
    return 12;
}

int32_t SoulSandOreFeature::getMinHeight() const {
    return 0;
}

int32_t SoulSandOreFeature::getMaxHeight() const {
    return 32;
}

const char *SoulSandOreFeature::name() const {
    return "nether_soulsand";
}
