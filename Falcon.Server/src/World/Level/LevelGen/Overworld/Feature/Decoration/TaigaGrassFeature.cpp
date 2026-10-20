#include "World/Level/LevelGen/Overworld/Feature/Decoration/TaigaGrassFeature.h"

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

const char *TaigaGrassFeature::name() const {
    return "minecraft:taiga_tall_grass_feature";
}

int32_t TaigaGrassFeature::getBase() const {
    return 8;
}

int32_t TaigaGrassFeature::getRandom() const {
    return 0;
}

void TaigaGrassFeature::place(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    mRandom.setSeed(x + y + z);
    manager.setBlockStateAt(x, y, z, mRandom.nextInt(7) == 0 ? shortGrassState() : fernState());
}
