#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyDarkOakTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <cmath>

LegacyDarkOakTree::LegacyDarkOakTree(float leafStartHeightMultiplier, int32_t baseLeafRadius)
        : mLeafStartHeightMultiplier(leafStartHeightMultiplier), mBaseLeafRadius(baseLeafRadius) {
}

TreeWoodType LegacyDarkOakTree::getType() const {
    return TreeWoodType::DARK_OAK;
}

void LegacyDarkOakTree::setRandomTreeHeight(IRandom &random) {
    mTreeHeight = random.nextInt(15) + 20;
}

void LegacyDarkOakTree::placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    if (mTreeHeight == 0)
        setRandomTreeHeight(random);

    const int32_t topSize = mTreeHeight - (int32_t) (mTreeHeight * mLeafStartHeightMultiplier);
    const int32_t leafRadius = mBaseLeafRadius + random.nextInt(2);

    setRandomTreeWithVines(random);
    placeTrunk(manager, x, y, z, random, getTreeHeight() - random.nextInt(3));

    placeLeaves(manager, topSize, leafRadius, x, y, z, random);
}

void LegacyDarkOakTree::placeTrunk(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                                   int32_t trunkHeight) {
    (void) random;

    manager.setBlockStateAt(x, y - 1, z, VanillaBlocks::DIRT().toBlockState());
    const int32_t radius = 2;

    for (int32_t yy = 0; yy < trunkHeight; ++yy) {
        for (int32_t xx = 0; xx < radius; xx++) {
            for (int32_t zz = 0; zz < radius; zz++) {
                if (overridable(manager.getBlockAt(x, y + yy, z))) {
                    manager.setBlockStateAt(x + xx, y + yy, z + zz, getTrunkBlockState());
                    if (mTreeWithVines)
                        addVinesAroundLog(manager, x + xx, y + yy, z + zz);
                }
            }
        }
    }
}

void LegacyDarkOakTree::placeLeaves(BlockManager &manager, int32_t topSize, int32_t leafRadius, int32_t x, int32_t y,
                                    int32_t z, IRandom &random) {
    int32_t radius = random.nextInt(2);
    int32_t maxR = 1;
    int32_t minR = 0;

    for (int32_t yy = 0; yy <= topSize; ++yy) {
        const int32_t yyy = y + mTreeHeight - yy;

        for (int32_t xx = x - radius; xx <= x + radius; ++xx) {
            const int32_t xOff = std::abs(xx - x);

            for (int32_t zz = z - radius; zz <= z + radius; ++zz) {
                const int32_t zOff = std::abs(zz - z);
                if (xOff == radius && zOff == radius && radius > 0)
                    continue;

                if (!isSolid(manager.getBlockAt(xx, yyy, zz)))
                    manager.setBlockStateAt(xx, yyy, zz, getLeafBlockState());
            }
        }

        if (radius >= maxR) {
            radius = minR;
            minR = 1;
            if (++maxR > leafRadius)
                maxR = leafRadius;
        } else {
            ++radius;
        }
    }
}
