#include "World/Level/LevelGen/Overworld/Feature/Decoration/BambooFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

namespace {

    const int32_t BAMBOO_SHOOTS_PER_FEATURE = 4;

    const BlockState &noLeavesState() {
        static const BlockState state = DecorationSupport::withState(
                VanillaBlocks::BAMBOO().toBlockState(), "bamboo_leaf_size", "no_leaves");
        return state;
    }

    const BlockState &smallLeavesState() {
        static const BlockState state = DecorationSupport::withState(
                VanillaBlocks::BAMBOO().toBlockState(), "bamboo_leaf_size", "small_leaves");
        return state;
    }

    const BlockState &largeLeavesState() {
        static const BlockState state = DecorationSupport::withState(
                VanillaBlocks::BAMBOO().toBlockState(), "bamboo_leaf_size", "large_leaves");
        return state;
    }

    const BlockState &podzolState() {
        static const BlockState state = VanillaBlocks::PODZOL().toBlockState();
        return state;
    }

}

const char *BambooFeature::name() const {
    return "minecraft:bamboo_then_podzol_feature";
}

int32_t BambooFeature::getBase() const {
    return 240;
}

int32_t BambooFeature::getRandom() const {
    return 80;
}

void BambooFeature::place(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    Level &level = manager.getLevel();
    if (!DecorationSupport::isBiomeBamboo(DecorationSupport::getBiomeAt(level, x, y, z)))
        return;

    mRandom.setSeed(x + y + z);

    for (int32_t i = 0; i < BAMBOO_SHOOTS_PER_FEATURE; i++) {
        int32_t px = x;
        int32_t pz = z;
        int32_t py = y;

        if (i > 0) {
            px += mRandom.nextInt(-3, 3);
            pz += mRandom.nextInt(-3, 3);
            py = level.getHeightAt(px, pz) + 1;
        }

        _placeBambooAt(manager, px, py, pz);
    }
}

void BambooFeature::_placeBambooAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    if (!DecorationSupport::isAir(manager.getBlockAt(x, y, z))
        || !IFeature::isSupportDirt(manager.getBlockAt(x, y - 1, z)))
        return;

    Level &level = manager.getLevel();
    const int32_t midX = x + 1;
    const int32_t midZ = z + 1;
    const int32_t radius = mRandom.nextInt(0, 2);

    for (int32_t offsetX = -radius - 1; offsetX <= radius; offsetX++) {
        for (int32_t offsetZ = -radius - 1; offsetZ <= radius; offsetZ++) {
            const float calcX = (float) offsetX + 0.5f;
            const float calcZ = (float) offsetZ + 0.5f;
            const float calcRadius = (float) radius + 0.8f;
            const int32_t px = midX + offsetX;
            const int32_t pz = midZ + offsetZ;

            if ((calcX * calcX) + (calcZ * calcZ) < (calcRadius * calcRadius))
                _placePodzolAt(manager, px, level.getHeightAt(px, pz), pz);
        }
    }

    const int32_t height = 5 + mRandom.nextInt(11);
    for (int32_t i = 0; i <= height; i++) {
        const int32_t diff = height - i;
        const BlockState &place = diff < 3 ? largeLeavesState() : (diff < 4 ? smallLeavesState() : noLeavesState());
        manager.setBlockStateAt(x, y + i, z, place);
    }
}

void BambooFeature::_placePodzolAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    if (IFeature::isSupportDirt(manager.getBlockAt(x, y, z)))
        manager.setBlockStateAt(x, y, z, podzolState());
}
