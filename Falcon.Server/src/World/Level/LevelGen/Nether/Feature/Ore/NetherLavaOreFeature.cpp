#include "World/Level/LevelGen/Nether/Feature/Ore/NetherLavaOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

const BlockState &NetherLavaOreFeature::getState(const BlockState &original) const {
    (void) original;

    static const BlockState state = VanillaBlocks::LAVA().toBlockState();
    return state;
}

int32_t NetherLavaOreFeature::getClusterCount() const {
    return 32;
}

int32_t NetherLavaOreFeature::getClusterSize() const {
    return 1;
}

int32_t NetherLavaOreFeature::getMinHeight() const {
    return 0;
}

int32_t NetherLavaOreFeature::getMaxHeight() const {
    return 32;
}

const char *NetherLavaOreFeature::name() const {
    return "nether_lava_ore";
}
