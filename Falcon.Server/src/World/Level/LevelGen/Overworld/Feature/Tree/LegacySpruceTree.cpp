#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacySpruceTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <cmath>

TreeWoodType LegacySpruceTree::getType() const {
    return TreeWoodType::SPRUCE;
}

void LegacySpruceTree::placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    mTreeHeight = random.nextInt(4) + 6;
    setRandomTreeWithVines(random);

    const int32_t topSize = getTreeHeight() - (1 + random.nextInt(2));
    const int32_t leafRadius = 2 + random.nextInt(2);

    placeTrunk(manager, x, y, z, random, getTreeHeight() - random.nextInt(3));

    placeLeaves(manager, topSize, leafRadius, x, y, z, random);
}

void LegacySpruceTree::placeLeaves(BlockManager &manager, int32_t topSize, int32_t leafRadius, int32_t x, int32_t y,
                                   int32_t z, IRandom &random) {
    int32_t radius = random.nextInt(2);
    int32_t maxR = 1;
    int32_t minR = 0;

    manager.setBlockStateAt(x, y - 1, z, VanillaBlocks::DIRT().toBlockState());

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
