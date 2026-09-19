#include "Level/Generator/Overworld/Feature/Tree/JungleBigTree.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Core/Math/MathConstants.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Feature/Tree/TreeMathHelper.h"

JungleBigTree::JungleBigTree(int32_t baseHeight, int32_t extraRandomHeight)
        : HugeTreeGenerator(baseHeight, extraRandomHeight,
                            withPillarAxis(VanillaBlocks::JUNGLE_LOG().toBlockState(), "y"),
                            VanillaBlocks::JUNGLE_LEAVES().toBlockState()) {
}

bool JungleBigTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const int32_t height = getHeight(random);

    if (!ensureGrowable(manager, random, x, y, z, height))
        return false;

    _createCrown(manager, x, y + height, z, 2);

    for (int32_t j = y + height - 2 - random.nextInt(4);
         (double) j > (double) y + (double) height / 2;
         j -= 2 + random.nextInt(4)) {
        const float angle = random.nextFloat() * (MathConstants::PI_F * 2.0f);
        int32_t branchX = 0;
        int32_t branchZ = 0;

        for (int32_t i1 = 0; i1 < 5; ++i1) {
            branchX = (int32_t) ((double) x + (double) (1.5f + TreeMathHelper::cos(angle) * (float) i1));
            branchZ = (int32_t) ((double) z + (double) (1.5f + TreeMathHelper::sin(angle) * (float) i1));
            manager.setBlockStateAt(branchX, j - 3 + i1 / 2, branchZ, mWoodMetadata);
        }

        const int32_t j2 = 1 + random.nextInt(2);

        for (int32_t k1 = j - j2; k1 <= j; ++k1) {
            const int32_t l1 = k1 - j;
            growLeavesLayer(manager, branchX, k1, branchZ, 1 - l1);
        }
    }

    for (int32_t i2 = 0; i2 < height; ++i2) {
        const int32_t trunkY = y + i2;

        if (canGrowInto(manager.getBlockAt(x, trunkY, z).mName)) {
            manager.setBlockStateAt(x, trunkY, z, mWoodMetadata);
            if (i2 > 0) {
                _placeVine(manager, random, x - 1, trunkY, z, 8);
                _placeVine(manager, random, x, trunkY, z - 1, 1);
            }
        }

        if (i2 < height - 1) {
            if (canGrowInto(manager.getBlockAt(x + 1, trunkY, z).mName)) {
                manager.setBlockStateAt(x + 1, trunkY, z, mWoodMetadata);

                if (i2 > 0) {
                    _placeVine(manager, random, x + 2, trunkY, z, 2);
                    _placeVine(manager, random, x + 1, trunkY, z - 1, 1);
                }
            }

            if (canGrowInto(manager.getBlockAt(x + 1, trunkY, z + 1).mName)) {
                manager.setBlockStateAt(x + 1, trunkY, z + 1, mWoodMetadata);

                if (i2 > 0) {
                    _placeVine(manager, random, x + 2, trunkY, z + 1, 2);
                    _placeVine(manager, random, x + 1, trunkY, z + 2, 4);
                }
            }

            if (canGrowInto(manager.getBlockAt(x, trunkY, z + 1).mName)) {
                manager.setBlockStateAt(x, trunkY, z + 1, mWoodMetadata);

                if (i2 > 0) {
                    _placeVine(manager, random, x - 1, trunkY, z + 1, 8);
                    _placeVine(manager, random, x, trunkY, z + 2, 4);
                }
            }
        }
    }

    return true;
}

void JungleBigTree::_placeVine(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z, int32_t meta) {
    if (random.nextInt(3) > 0 && manager.getBlockAt(x, y, z).mName == "minecraft:air")
        manager.setBlockStateAt(x, y, z, getVineState(meta));
}

void JungleBigTree::_createCrown(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t radius) {
    for (int32_t j = -2; j <= 0; ++j)
        growLeavesLayerStrict(manager, x, y + j, z, radius + 1 - j);
}
