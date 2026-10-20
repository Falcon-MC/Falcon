#include "World/Level/LevelGen/Overworld/Feature/Decoration/DryGrassDiscFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"

#include <chrono>

namespace {

    const BlockState &shortDryGrassState() {
        static const BlockState state = VanillaBlocks::SHORT_DRY_GRASS().toBlockState();
        return state;
    }

    const BlockState &tallDryGrassState() {
        static const BlockState state = VanillaBlocks::TALL_DRY_GRASS().toBlockState();
        return state;
    }

    int64_t currentTimeMillis() {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    }

}

const char *DryGrassDiscFeature::name() const {
    return "minecraft:scatter_dry_grass_feature";
}

BlockState DryGrassDiscFeature::getSourceBlock() const {
    SimpleRandom random(currentTimeMillis());
    return random.nextInt(3) == 0 ? tallDryGrassState() : shortDryGrassState();
}

int32_t DryGrassDiscFeature::getMinRadius() const {
    return 3;
}

int32_t DryGrassDiscFeature::getMaxRadius() const {
    return 4;
}

double DryGrassDiscFeature::getProbability() const {
    return (double) 0.4f;
}

int32_t DryGrassDiscFeature::getBase() const {
    return -10;
}

int32_t DryGrassDiscFeature::getRandom() const {
    return 12;
}
