#include "World/Level/LevelGen/Overworld/Feature/Decoration/MelonFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

namespace {

    const BlockState &melonState() {
        static const BlockState state = VanillaBlocks::MELON_BLOCK().toBlockState();
        return state;
    }

}

const char *MelonFeature::name() const {
    return "minecraft:jungle_after_surface_melon_feature";
}

int32_t MelonFeature::getBase() const {
    return -1500;
}

int32_t MelonFeature::getRandom() const {
    return 1520;
}

void MelonFeature::place(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    manager.setBlockStateAt(x, y, z, melonState());
}
