#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacyForestRockFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

namespace {

    const BlockState &mossyCobblestoneState() {
        static const BlockState state = VanillaBlocks::MOSSY_COBBLESTONE().toBlockState();
        return state;
    }

}

const char *LegacyForestRockFeature::name() const {
    return "minecraft:legacy:forest_rock_feature";
}

int32_t LegacyForestRockFeature::getBase() const {
    return -1;
}

int32_t LegacyForestRockFeature::getRandom() const {
    return 2;
}

void LegacyForestRockFeature::place(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    mRandom.setSeed(x + y + z);

    const int32_t sizeX = 3 + mRandom.nextInt(1);
    const int32_t sizeZ = 3 + mRandom.nextInt(1);

    for (int32_t offsetX = 0; offsetX < sizeX; offsetX++) {
        for (int32_t offsetY = -1; offsetY <= 1; offsetY++) {
            for (int32_t offsetZ = 0; offsetZ < sizeX; offsetZ++) {
                const bool corner = (offsetX == 0 || offsetX == sizeX - 1)
                                    && (offsetZ == 0 || offsetZ == sizeZ - 1)
                                    && offsetY != 0;
                if (!corner)
                    manager.setBlockStateAt(x + offsetX, y + offsetY, z + offsetZ, mossyCobblestoneState());
            }
        }
    }
}

bool LegacyForestRockFeature::isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y,
                                             int32_t z) const {
    return IFeature::isSupportDirt(support)
           && DecorationSupport::isBiomeTaiga(DecorationSupport::getBiomeAt(level, x, y, z));
}
