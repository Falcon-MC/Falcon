#include "World/Level/LevelGen/Overworld/Feature/Decoration/FireflyBushWaterClusterFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

namespace {

    const BlockState &fireflyBushState() {
        static const BlockState state = VanillaBlocks::FIREFLY_BUSH().toBlockState();
        return state;
    }

}

const char *FireflyBushWaterClusterFeature::name() const {
    return "minecraft:firefly_bush_water_cluster_feature";
}

BlockState FireflyBushWaterClusterFeature::getSourceBlock() const {
    return fireflyBushState();
}

int32_t FireflyBushWaterClusterFeature::getMinRadius() const {
    return 3;
}

int32_t FireflyBushWaterClusterFeature::getMaxRadius() const {
    return 4;
}

double FireflyBushWaterClusterFeature::getProbability() const {
    return (double) 0.5f;
}

int32_t FireflyBushWaterClusterFeature::getBase() const {
    return -10;
}

int32_t FireflyBushWaterClusterFeature::getRandom() const {
    return 12;
}

bool FireflyBushWaterClusterFeature::isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y,
                                                    int32_t z) const {
    return IFeature::isSupportDirt(support) && DecorationSupport::reedsSupportValid(level, support, x, y, z);
}
