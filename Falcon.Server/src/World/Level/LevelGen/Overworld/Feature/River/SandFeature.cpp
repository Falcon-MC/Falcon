#include "World/Level/LevelGen/Overworld/Feature/River/SandFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {
    const BlockState &sandState() {
        static const BlockState state = VanillaBlocks::SAND().toBlockState();
        return state;
    }
}

const BlockState &SandFeature::getSourceBlock() const {
    return sandState();
}

int32_t SandFeature::getMinRadius() const {
    return 2;
}

int32_t SandFeature::getMaxRadius() const {
    return 7;
}

int32_t SandFeature::getRadiusY() const {
    return 2;
}

int32_t SandFeature::getBase() const {
    return 5;
}

const char *SandFeature::name() const {
    return "minecraft:minecraft:overworld_surface_sand_feature";
}
