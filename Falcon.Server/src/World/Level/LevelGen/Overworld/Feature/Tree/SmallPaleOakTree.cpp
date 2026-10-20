#include "World/Level/LevelGen/Overworld/Feature/Tree/SmallPaleOakTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"
#include "World/Level/Level.h"

#include <cmath>

namespace {

    BlockState paleHangingMossState(bool tip) {
        BlockState state = VanillaBlocks::PALE_HANGING_MOSS().toBlockState();
        state.mStates.putByte("tip", tip ? 1 : 0);
        return BlockState(state.mName, state.mStates);
    }

}

SmallPaleOakTree::SmallPaleOakTree(int32_t minTreeHeight, int32_t maxTreeHeight)
        : mMinTreeHeight(minTreeHeight), mMaxTreeHeight(maxTreeHeight) {
}

bool SmallPaleOakTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    int32_t i = random.nextInt(mMaxTreeHeight) + mMinTreeHeight;
    bool flag = true;

    if (y < getMinHeight() || y + i + 1 >= getMaxHeight())
        return false;

    for (int32_t j = y; j <= y + 1 + i; ++j) {
        int32_t k = 1;

        if (j == y)
            k = 0;

        if (j >= y + 1 + i - 2)
            k = 2;

        for (int32_t l = x - k; l <= x + k && flag; ++l) {
            for (int32_t i1 = z - k; i1 <= z + k && flag; ++i1) {
                if (j >= getMinHeight() && j < getMaxHeight()) {
                    if (!canGrowInto(manager.getBlockAt(l, j, i1).mName))
                        flag = false;
                } else {
                    flag = false;
                }
            }
        }
    }

    if (!flag)
        return false;

    const std::string ground = manager.getBlockAt(x, y - 1, z).mName;
    if ((ground != "minecraft:grass_block" && ground != "minecraft:dirt" && ground != "minecraft:farmland")
        || y >= getMaxHeight() - i - 1)
        return false;

    setDirtAt(manager, x, y - 1, z);

    const BlockState metaLeaves = VanillaBlocks::PALE_OAK_LEAVES().toBlockState();

    for (int32_t i3 = y - 3 + i; i3 <= y + i; ++i3) {
        const int32_t i4 = i3 - (y + i);
        const int32_t j1 = 1 - i4 / 2;

        for (int32_t k1 = x - j1; k1 <= x + j1; ++k1) {
            const int32_t l1 = k1 - x;

            for (int32_t i2 = z - j1; i2 <= z + j1; ++i2) {
                const int32_t j2 = i2 - z;

                if (std::abs(l1) != j1 || std::abs(j2) != j1 || (random.nextInt(2) != 0 && i4 != 0)) {
                    const std::string identifier = manager.getBlockAt(k1, i3, i2).mName;

                    if (identifier == "minecraft:air" || isLeaves(identifier)
                        || identifier == "minecraft:pale_hanging_moss") {
                        manager.setBlockStateAt(k1, i3, i2, metaLeaves);

                        SimpleRandom mossRandom(manager.getLevel().getSeed() + k1 + i3 + i2);
                        if (mossRandom.nextInt(2) == 0) {
                            const int32_t depth = mossRandom.nextInt(1, 6);
                            for (int32_t j = 1; j < depth; i++) {
                                if (manager.getBlockAt(k1, i3 - i, i2).mName == "minecraft:air") {
                                    if (i == depth - 1)
                                        manager.setBlockStateAt(k1, i3 - i, i2, paleHangingMossState(true));
                                    else
                                        manager.setBlockStateAt(k1, i3 - i, i2, paleHangingMossState(false));
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}
