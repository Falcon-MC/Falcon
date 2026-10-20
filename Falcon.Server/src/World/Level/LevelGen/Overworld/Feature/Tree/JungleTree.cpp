#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <cmath>

JungleTree::JungleTree(int32_t minTreeHeight, int32_t maxTreeHeight)
        : mMinTreeHeight(minTreeHeight), mMaxTreeHeight(maxTreeHeight) {
}

bool JungleTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const int32_t i = random.nextInt(mMaxTreeHeight) + mMinTreeHeight;
    const bool treeWithVines = random.nextInt(TREE_WITH_VINES_CHANCE) == 0;
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

    const BlockState metaWood = withPillarAxis(VanillaBlocks::JUNGLE_LOG().toBlockState(), "y");
    const BlockState metaLeaves = VanillaBlocks::JUNGLE_LEAVES().toBlockState();

    for (int32_t i3 = y - 3 + i; i3 <= y + i; ++i3) {
        const int32_t i4 = i3 - (y + i);
        const int32_t j1 = 1 - i4 / 2;

        for (int32_t k1 = x - j1; k1 <= x + j1; ++k1) {
            const int32_t l1 = k1 - x;

            for (int32_t i2 = z - j1; i2 <= z + j1; ++i2) {
                const int32_t j2 = i2 - z;

                if (std::abs(l1) != j1 || std::abs(j2) != j1 || (random.nextInt(2) != 0 && i4 != 0)) {
                    const std::string identifier = manager.getBlockAt(k1, i3, i2).mName;

                    if (identifier == "minecraft:air" || isLeaves(identifier) || identifier == "minecraft:vine")
                        manager.setBlockStateAt(k1, i3, i2, metaLeaves);
                }
            }
        }
    }

    for (int32_t j3 = 0; j3 < i; ++j3) {
        const std::string identifier = manager.getBlockAt(x, y + j3, z).mName;

        if (identifier == "minecraft:air" || isLeaves(identifier) || identifier == "minecraft:vine") {
            manager.setBlockStateAt(x, y + j3, z, metaWood);

            if (j3 > 0) {
                if (treeWithVines) {
                    addVinesAroundLog(manager, x, y + j3, z);
                } else {
                    if (random.nextInt(3) > 0 && _isAirBlock(manager, x - 1, y + j3, z))
                        _addVine(manager, x - 1, y + j3, z, 8);

                    if (random.nextInt(3) > 0 && _isAirBlock(manager, x + 1, y + j3, z))
                        _addVine(manager, x + 1, y + j3, z, 2);

                    if (random.nextInt(3) > 0 && _isAirBlock(manager, x, y + j3, z - 1))
                        _addVine(manager, x, y + j3, z - 1, 1);

                    if (random.nextInt(3) > 0 && _isAirBlock(manager, x, y + j3, z + 1))
                        _addVine(manager, x, y + j3, z + 1, 4);
                }
            }
        }
    }

    for (int32_t k3 = y - 3 + i; k3 <= y + i; ++k3) {
        const int32_t j4 = k3 - (y + i);
        const int32_t k4 = 2 - j4 / 2;

        for (int32_t l4 = x - k4; l4 <= x + k4; ++l4) {
            for (int32_t i5 = z - k4; i5 <= z + k4; ++i5) {
                if (isLeaves(manager.getBlockAt(l4, k3, i5).mName)) {
                    if (random.nextInt(4) == 0 && manager.getBlockAt(l4 - 1, k3, i5).mName == "minecraft:air")
                        _addHangingVine(manager, l4 - 1, k3, i5, 8);

                    if (random.nextInt(4) == 0 && manager.getBlockAt(l4 + 1, k3, i5).mName == "minecraft:air")
                        _addHangingVine(manager, l4 + 1, k3, i5, 2);

                    if (random.nextInt(4) == 0 && manager.getBlockAt(l4, k3, i5 - 1).mName == "minecraft:air")
                        _addHangingVine(manager, l4, k3, i5 - 1, 1);

                    if (random.nextInt(4) == 0 && manager.getBlockAt(l4, k3, i5 + 1).mName == "minecraft:air")
                        _addHangingVine(manager, l4, k3, i5 + 1, 4);
                }
            }
        }
    }

    if (random.nextInt(5) == 0 && i > 5) {
        for (int32_t l3 = 0; l3 < 2; ++l3) {
            for (const TreeBlockFace face: TreeBlockFaces::HORIZONTAL_PLANE) {
                if (random.nextInt(4 - l3) == 0) {
                    const TreeBlockFace opposite = TreeBlockFaces::getOpposite(face);
                    _placeCocoa(manager, random.nextInt(2),
                                x + TreeBlockFaces::getXOffset(opposite),
                                y + i - 5 + l3,
                                z + TreeBlockFaces::getZOffset(opposite), face);
                }
            }
        }
    }

    return true;
}

void JungleTree::_placeCocoa(BlockManager &manager, int32_t age, int32_t x, int32_t y, int32_t z, TreeBlockFace side) {
    BlockState state = VanillaBlocks::COCOA().toBlockState();
    state.mStates.putInt("age", age);
    state.mStates.putInt("direction", TreeBlockFaces::getHorizontalIndex(side));
    manager.setBlockStateAt(x, y, z, BlockState(state.mName, state.mStates));
}

void JungleTree::_addVine(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t meta) {
    manager.setBlockStateAt(x, y, z, getVineState(meta));
}

void JungleTree::_addHangingVine(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t meta) {
    _addVine(manager, x, y, z, meta);
    int32_t remaining = 4;
    int32_t currentY = y - 1;

    while (remaining > 0 && manager.getBlockAt(x, currentY, z).mName == "minecraft:air") {
        _addVine(manager, x, currentY, z, meta);
        currentY--;
        --remaining;
    }
}

bool JungleTree::_isAirBlock(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    return manager.getBlockAt(x, y, z).mName == "minecraft:air";
}
