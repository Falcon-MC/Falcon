#include "World/Level/LevelGen/Overworld/Feature/Decoration/EyeblossomFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

namespace {

    const BlockState &closedEyeblossomState() {
        static const BlockState state = VanillaBlocks::CLOSED_EYEBLOSSOM().toBlockState();
        return state;
    }

}

const char *EyeblossomFeature::name() const {
    return "minecraft:scatter_eyeblossom_feature";
}

int32_t EyeblossomFeature::getBase() const {
    return 10;
}

int32_t EyeblossomFeature::getRandom() const {
    return 0;
}

void EyeblossomFeature::place(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    manager.setBlockStateAt(x, y, z, closedEyeblossomState());
}

bool EyeblossomFeature::isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y, int32_t z) const {
    return IFeature::isSupportDirt(support)
           && DecorationSupport::isBiomePaleGarden(DecorationSupport::getBiomeAt(level, x, y, z));
}
