#include "Level/Generator/Overworld/Feature/Decoration/BushFeature.h"

#include "Block/Blocks/VanillaBlocks.h"

namespace {

    const BlockState &bushState() {
        static const BlockState state = VanillaBlocks::BUSH().toBlockState();
        return state;
    }

}

const char *BushFeature::name() const {
    return "minecraft:scatter_bush_feature";
}

BlockState BushFeature::getSourceBlock() const {
    return bushState();
}

int32_t BushFeature::getMinRadius() const {
    return 1;
}

int32_t BushFeature::getMaxRadius() const {
    return 1;
}

double BushFeature::getProbability() const {
    return 0.5;
}

int32_t BushFeature::getBase() const {
    return -3;
}

int32_t BushFeature::getRandom() const {
    return 4;
}

bool BushFeature::isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y, int32_t z) const {
    (void) level;
    (void) x;
    (void) y;
    (void) z;

    return IFeature::isSupportDirt(support);
}
