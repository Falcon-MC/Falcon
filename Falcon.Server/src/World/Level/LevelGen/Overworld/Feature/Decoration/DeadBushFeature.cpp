#include "World/Level/LevelGen/Overworld/Feature/Decoration/DeadBushFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {

    const BlockState &deadBushState() {
        static const BlockState state = VanillaBlocks::DEADBUSH().toBlockState();
        return state;
    }

}

const char *DeadBushFeature::name() const {
    return "minecraft:dead_bush_feature";
}

BlockState DeadBushFeature::getSourceBlock() const {
    return deadBushState();
}

int32_t DeadBushFeature::getMinRadius() const {
    return 3;
}

int32_t DeadBushFeature::getMaxRadius() const {
    return 4;
}

double DeadBushFeature::getProbability() const {
    return (double) 0.2f;
}

int32_t DeadBushFeature::getBase() const {
    return -10;
}

int32_t DeadBushFeature::getRandom() const {
    return 12;
}
