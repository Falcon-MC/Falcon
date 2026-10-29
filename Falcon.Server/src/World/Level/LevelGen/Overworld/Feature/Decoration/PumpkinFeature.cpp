#include "World/Level/LevelGen/Overworld/Feature/Decoration/PumpkinFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

namespace {

    const BlockState &pumpkinState() {
        static const BlockState state = VanillaBlocks::PUMPKIN().toBlockState();
        return state;
    }

}

const char *PumpkinFeature::name() const {
    return "minecraft:pumpkin_feature";
}

int32_t PumpkinFeature::getBase() const {
    return -2000;
}

int32_t PumpkinFeature::getRandom() const {
    return 2015;
}

void PumpkinFeature::place(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    manager.setBlockStateAt(x, y, z, pumpkinState());
}
