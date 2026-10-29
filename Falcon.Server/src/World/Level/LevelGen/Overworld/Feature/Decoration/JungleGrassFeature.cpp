#include "World/Level/LevelGen/Overworld/Feature/Decoration/JungleGrassFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

namespace {

    const BlockState &shortGrassState() {
        static const BlockState state = VanillaBlocks::SHORT_GRASS().toBlockState();
        return state;
    }

    const BlockState &fernState() {
        static const BlockState state = VanillaBlocks::FERN().toBlockState();
        return state;
    }

}

const char *JungleGrassFeature::name() const {
    return "minecraft:jungle_tall_grass_feature";
}

int32_t JungleGrassFeature::getBase() const {
    return 100;
}

int32_t JungleGrassFeature::getRandom() const {
    return 10;
}

void JungleGrassFeature::place(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    manager.setBlockStateAt(x, y, z, mRandom.nextInt(7) == 0 ? fernState() : shortGrassState());
}
