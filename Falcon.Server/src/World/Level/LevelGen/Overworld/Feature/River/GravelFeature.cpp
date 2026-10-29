#include "World/Level/LevelGen/Overworld/Feature/River/GravelFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &gravelState() {
        static const BlockState state = VanillaBlocks::GRAVEL().toBlockState();
        return state;
    }
}

const BlockState &GravelFeature::getSourceBlock() const {
    return gravelState();
}

int32_t GravelFeature::getMinRadius() const {
    return 2;
}

int32_t GravelFeature::getMaxRadius() const {
    return 5;
}

int32_t GravelFeature::getRadiusY() const {
    return 2;
}

int32_t GravelFeature::getBase() const {
    return 3;
}

const char *GravelFeature::name() const {
    return "minecraft:overworld_surface_gravel_feature";
}
