#include "World/Level/LevelGen/Nether/Feature/Ore/NetherQuartzOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

const BlockState &NetherQuartzOreFeature::getState(const BlockState &original) const {
    (void) original;

    static const BlockState state = VanillaBlocks::QUARTZ_ORE().toBlockState();
    return state;
}

int32_t NetherQuartzOreFeature::getClusterCount() const {
    return 20;
}

int32_t NetherQuartzOreFeature::getClusterSize() const {
    return 14;
}

int32_t NetherQuartzOreFeature::getMinHeight() const {
    return 10;
}

int32_t NetherQuartzOreFeature::getMaxHeight() const {
    return 117;
}

const char *NetherQuartzOreFeature::name() const {
    return "nether_quartz";
}
