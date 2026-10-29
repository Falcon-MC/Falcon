#include "World/Level/LevelGen/Overworld/Feature/Decoration/SwampFlowerDiscFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

namespace {

    const BlockState &blueOrchidState() {
        static const BlockState state = VanillaBlocks::BLUE_ORCHID().toBlockState();
        return state;
    }

}

const char *SwampFlowerDiscFeature::name() const {
    return "minecraft:scatter_swamp_flower_feature";
}

BlockState SwampFlowerDiscFeature::getSourceBlock() const {
    return blueOrchidState();
}

int32_t SwampFlowerDiscFeature::getMinRadius() const {
    return 1;
}

int32_t SwampFlowerDiscFeature::getMaxRadius() const {
    return 2;
}

double SwampFlowerDiscFeature::getProbability() const {
    return (double) 0.7f;
}

int32_t SwampFlowerDiscFeature::getBase() const {
    return -10;
}

int32_t SwampFlowerDiscFeature::getRandom() const {
    return 12;
}

bool SwampFlowerDiscFeature::isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y,
                                            int32_t z) const {
    return IFeature::isSupportDirt(support) && DecorationSupport::reedsSupportValid(level, support, x, y, z);
}
