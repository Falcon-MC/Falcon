#include "World/Level/LevelGen/Overworld/Feature/Decoration/TallGrassDoublePlantFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"

namespace {

    const BlockState &lowerState() {
        static const BlockState state = DecorationSupport::withByteState(
                VanillaBlocks::TALL_GRASS().toBlockState(), "upper_block_bit", false);
        return state;
    }

    const BlockState &upperState() {
        static const BlockState state = DecorationSupport::withByteState(
                VanillaBlocks::TALL_GRASS().toBlockState(), "upper_block_bit", true);
        return state;
    }

}

const char *TallGrassDoublePlantFeature::name() const {
    return "minecraft:grass_double_plant_patch_feature";
}

int32_t TallGrassDoublePlantFeature::getBase() const {
    return 5;
}

int32_t TallGrassDoublePlantFeature::getRandom() const {
    return 0;
}

void TallGrassDoublePlantFeature::place(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    if (!DecorationSupport::isAir(manager.getBlockAt(x, y + 1, z)))
        return;

    manager.setBlockStateAt(x, y, z, lowerState());
    manager.setBlockStateAt(x, y + 1, z, upperState());
}
