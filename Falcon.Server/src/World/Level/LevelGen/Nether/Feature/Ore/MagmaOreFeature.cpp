#include "World/Level/LevelGen/Nether/Feature/Ore/MagmaOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

const BlockState &MagmaOreFeature::getState(const BlockState &original) const {
    (void) original;

    static const BlockState state = VanillaBlocks::MAGMA().toBlockState();
    return state;
}

int32_t MagmaOreFeature::getClusterCount() const {
    return 9;
}

int32_t MagmaOreFeature::getClusterSize() const {
    return 28;
}

int32_t MagmaOreFeature::getMinHeight() const {
    return 23;
}

int32_t MagmaOreFeature::getMaxHeight() const {
    return 36;
}

const char *MagmaOreFeature::name() const {
    return "nether_magma";
}
