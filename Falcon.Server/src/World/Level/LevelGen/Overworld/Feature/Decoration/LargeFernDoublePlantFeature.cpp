#include "World/Level/LevelGen/Overworld/Feature/Decoration/LargeFernDoublePlantFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"

namespace {

    const BlockState &lowerState() {
        static const BlockState state = DecorationSupport::withByteState(
                VanillaBlocks::LARGE_FERN().toBlockState(), "upper_block_bit", false);
        return state;
    }

    const BlockState &upperState() {
        static const BlockState state = DecorationSupport::withByteState(
                VanillaBlocks::LARGE_FERN().toBlockState(), "upper_block_bit", true);
        return state;
    }

}

const char *LargeFernDoublePlantFeature::name() const {
    return "minecraft:fern_double_plant_patch_feature";
}

int32_t LargeFernDoublePlantFeature::getBase() const {
    return 5;
}

int32_t LargeFernDoublePlantFeature::getRandom() const {
    return 0;
}

void LargeFernDoublePlantFeature::place(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    if (!DecorationSupport::isAir(manager.getBlockAt(x, y + 1, z)))
        return;

    manager.setBlockStateAt(x, y, z, lowerState());
    manager.setBlockStateAt(x, y + 1, z, upperState());
}
