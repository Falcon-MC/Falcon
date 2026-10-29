#include "World/Level/LevelGen/Overworld/Feature/Tree/DarkOakTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBlockFace.h"

#include <cmath>

bool DarkOakTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const int32_t i = random.nextInt(3) + random.nextInt(2) + 6;
    const bool treeWithVines = random.nextInt(TREE_WITH_VINES_CHANCE) == 0;

    if (y < 1 || y + i + 1 >= 256)
        return false;

    if (!canGrowInto(manager.getBlockAt(x, y - 1, z).mName))
        return false;

    if (!_placeTreeOfHeight(manager, x, y, z, i))
        return false;

    setDirtAt(manager, x, y - 1, z);
    setDirtAt(manager, x + 1, y - 1, z);
    setDirtAt(manager, x, y - 1, z + 1);
    setDirtAt(manager, x + 1, y - 1, z + 1);

    const TreeBlockFace facing = TreeBlockFaces::HORIZONTAL_PLANE[random.nextInt(4)];
    const int32_t i1 = i - random.nextInt(4);
    int32_t j1 = 2 - random.nextInt(3);
    int32_t k1 = x;
    int32_t l1 = z;
    const int32_t i2 = y + i - 1;

    for (int32_t j2 = 0; j2 < i; ++j2) {
        if (j2 >= i1 && j1 > 0) {
            k1 += TreeBlockFaces::getXOffset(facing);
            l1 += TreeBlockFaces::getZOffset(facing);
            --j1;
        }

        const int32_t k2 = y + j2;
        const std::string identifier = manager.getBlockAt(k1, k2, l1).mName;

        if (canGrowInto(identifier) || isLeaves(identifier)) {
            _placeLogAt(manager, k1, k2, l1, treeWithVines);
            _placeLogAt(manager, k1 + 1, k2, l1, treeWithVines);
            _placeLogAt(manager, k1, k2, l1 + 1, treeWithVines);
            _placeLogAt(manager, k1 + 1, k2, l1 + 1, treeWithVines);
        }
    }

    for (int32_t i3 = -2; i3 <= 0; ++i3) {
        for (int32_t l3 = -2; l3 <= 0; ++l3) {
            int32_t k4 = -1;
            _placeLeafAt(manager, k1 + i3, i2 + k4, l1 + l3);
            _placeLeafAt(manager, 1 + k1 - i3, i2 + k4, l1 + l3);
            _placeLeafAt(manager, k1 + i3, i2 + k4, 1 + l1 - l3);
            _placeLeafAt(manager, 1 + k1 - i3, i2 + k4, 1 + l1 - l3);

            if ((i3 > -2 || l3 > -1) && (i3 != -1 || l3 != -2)) {
                k4 = 1;
                _placeLeafAt(manager, k1 + i3, i2 + k4, l1 + l3);
                _placeLeafAt(manager, 1 + k1 - i3, i2 + k4, l1 + l3);
                _placeLeafAt(manager, k1 + i3, i2 + k4, 1 + l1 - l3);
                _placeLeafAt(manager, 1 + k1 - i3, i2 + k4, 1 + l1 - l3);
            }
        }
    }

    if (random.nextBoolean()) {
        _placeLeafAt(manager, k1, i2 + 2, l1);
        _placeLeafAt(manager, k1 + 1, i2 + 2, l1);
        _placeLeafAt(manager, k1 + 1, i2 + 2, l1 + 1);
        _placeLeafAt(manager, k1, i2 + 2, l1 + 1);
    }

    for (int32_t j3 = -3; j3 <= 4; ++j3) {
        for (int32_t i4 = -3; i4 <= 4; ++i4) {
            if ((j3 != -3 || i4 != -3) && (j3 != -3 || i4 != 4) && (j3 != 4 || i4 != -3) && (j3 != 4 || i4 != 4)
                && (std::abs(j3) < 3 || std::abs(i4) < 3))
                _placeLeafAt(manager, k1 + j3, i2, l1 + i4);
        }
    }

    for (int32_t k3 = -1; k3 <= 2; ++k3) {
        for (int32_t j4 = -1; j4 <= 2; ++j4) {
            if ((k3 < 0 || k3 > 1 || j4 < 0 || j4 > 1) && random.nextInt(3) <= 0) {
                const int32_t l4 = random.nextInt(3) + 2;

                for (int32_t i5 = 0; i5 < l4; ++i5)
                    _placeLogAt(manager, x + k3, i2 - i5 - 1, z + j4, false);

                for (int32_t j5 = -1; j5 <= 1; ++j5) {
                    for (int32_t l2 = -1; l2 <= 1; ++l2)
                        _placeLeafAt(manager, k1 + k3 + j5, i2, l1 + j4 + l2);
                }

                for (int32_t k5 = -2; k5 <= 2; ++k5) {
                    for (int32_t l5 = -2; l5 <= 2; ++l5) {
                        if (std::abs(k5) != 2 || std::abs(l5) != 2)
                            _placeLeafAt(manager, k1 + k3 + k5, i2 - 1, l1 + j4 + l5);
                    }
                }
            }
        }
    }

    return true;
}

bool DarkOakTree::_placeTreeOfHeight(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t height) {
    for (int32_t l = 0; l <= height + 1; ++l) {
        int32_t i1 = 1;

        if (l == 0)
            i1 = 0;

        if (l >= height - 1)
            i1 = 2;

        for (int32_t j1 = -i1; j1 <= i1; ++j1) {
            for (int32_t k1 = -i1; k1 <= i1; ++k1) {
                if (!canGrowInto(manager.getBlockAt(x + j1, y + l, z + k1).mName))
                    return false;
            }
        }
    }

    return true;
}

void DarkOakTree::_placeLogAt(BlockManager &manager, int32_t x, int32_t y, int32_t z, bool treeWithVines) {
    if (canGrowInto(manager.getBlockAt(x, y, z).mName)) {
        manager.setBlockStateAt(x, y, z, withPillarAxis(VanillaBlocks::DARK_OAK_LOG().toBlockState(), "y"));
        if (treeWithVines)
            addVinesAroundLog(manager, x, y, z);
    }
}

void DarkOakTree::_placeLeafAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    const std::string identifier = manager.getBlockAt(x, y, z).mName;
    if (identifier == "minecraft:air" || identifier == "minecraft:vine")
        manager.setBlockStateAt(x, y, z, VanillaBlocks::DARK_OAK_LEAVES().toBlockState());
}
