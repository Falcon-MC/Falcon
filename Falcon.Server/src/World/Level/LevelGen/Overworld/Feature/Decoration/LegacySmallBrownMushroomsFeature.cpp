#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacySmallBrownMushroomsFeature.h"

#include "World/Level/Block/VanillaBlocks.h"

namespace {

    const BlockState &brownMushroomState() {
        static const BlockState state = VanillaBlocks::BROWN_MUSHROOM().toBlockState();
        return state;
    }

}

const char *LegacySmallBrownMushroomsFeature::name() const {
    return "minecraft:scatter_brown_mushroom_feature";
}

BlockState LegacySmallBrownMushroomsFeature::getSourceBlock() const {
    return brownMushroomState();
}

int32_t LegacySmallBrownMushroomsFeature::getBase() const {
    return -3;
}

int32_t LegacySmallBrownMushroomsFeature::getRandom() const {
    return 4;
}
