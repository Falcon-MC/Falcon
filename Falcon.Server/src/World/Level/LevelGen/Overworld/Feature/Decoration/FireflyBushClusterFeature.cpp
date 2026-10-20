#include "World/Level/LevelGen/Overworld/Feature/Decoration/FireflyBushClusterFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

namespace {

    const BlockState &fireflyBushState() {
        static const BlockState state = VanillaBlocks::FIREFLY_BUSH().toBlockState();
        return state;
    }

}

const char *FireflyBushClusterFeature::name() const {
    return "minecraft:firefly_bush_cluster_feature";
}

BlockState FireflyBushClusterFeature::getSourceBlock() const {
    return fireflyBushState();
}

int32_t FireflyBushClusterFeature::getMinRadius() const {
    return 1;
}

int32_t FireflyBushClusterFeature::getMaxRadius() const {
    return 1;
}

double FireflyBushClusterFeature::getProbability() const {
    return (double) 0.8f;
}

int32_t FireflyBushClusterFeature::getBase() const {
    return -10;
}

int32_t FireflyBushClusterFeature::getRandom() const {
    return 12;
}

bool FireflyBushClusterFeature::isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y,
                                               int32_t z) const {
    return IFeature::isSupportDirt(support) && DecorationSupport::reedsSupportValid(level, support, x, y, z);
}
