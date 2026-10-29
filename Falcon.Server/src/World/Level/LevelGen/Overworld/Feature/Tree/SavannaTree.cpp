#include "World/Level/LevelGen/Overworld/Feature/Tree/SavannaTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBlockFace.h"

#include <cmath>

bool SavannaTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const int32_t i = random.nextInt(3) + random.nextInt(3) + 5;
    bool flag = true;

    if (y < 1 || y + i + 1 > 256)
        return false;

    for (int32_t j = y; j <= y + 1 + i; ++j) {
        int32_t k = 1;

        if (j == y)
            k = 0;

        if (j >= y + 1 + i - 2)
            k = 2;

        for (int32_t l = x - k; l <= x + k && flag; ++l) {
            for (int32_t i1 = z - k; i1 <= z + k && flag; ++i1) {
                if (j >= 0 && j < 256) {
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
    if ((ground != "minecraft:grass_block" && ground != "minecraft:dirt") || y >= 256 - i - 1)
        return false;

    setDirtAt(manager, x, y - 1, z);

    const TreeBlockFace face = TreeBlockFaces::HORIZONTAL_PLANE[random.nextInt(4)];
    const int32_t k2 = i - random.nextInt(4) - 1;
    int32_t l2 = 3 - random.nextInt(3);
    int32_t i3 = x;
    int32_t j1 = z;
    int32_t k1 = 0;

    for (int32_t l1 = 0; l1 < i; ++l1) {
        const int32_t i2 = y + l1;

        if (l1 >= k2 && l2 > 0) {
            i3 += TreeBlockFaces::getXOffset(face);
            j1 += TreeBlockFaces::getZOffset(face);
            --l2;
        }

        const std::string identifier = manager.getBlockAt(i3, i2, j1).mName;
        if (identifier == "minecraft:air" || isLeaves(identifier)) {
            _placeLogAt(manager, i3, i2, j1);
            k1 = i2;
        }
    }

    int32_t crownX = i3;
    int32_t crownY = k1;
    int32_t crownZ = j1;

    for (int32_t j3 = -3; j3 <= 3; ++j3) {
        for (int32_t i4 = -3; i4 <= 3; ++i4) {
            if (std::abs(j3) != 3 || std::abs(i4) != 3)
                _placeLeafAt(manager, crownX + j3, crownY, crownZ + i4);
        }
    }

    crownY = crownY + 1;

    for (int32_t k3 = -1; k3 <= 1; ++k3) {
        for (int32_t j4 = -1; j4 <= 1; ++j4)
            _placeLeafAt(manager, crownX + k3, crownY, crownZ + j4);
    }

    _placeLeafAt(manager, crownX + 2, crownY, crownZ);
    _placeLeafAt(manager, crownX - 2, crownY, crownZ);
    _placeLeafAt(manager, crownX, crownY, crownZ + 2);
    _placeLeafAt(manager, crownX, crownY, crownZ - 2);

    i3 = x;
    j1 = z;
    const TreeBlockFace face1 = TreeBlockFaces::HORIZONTAL_PLANE[random.nextInt(4)];

    if (face1 != face) {
        const int32_t l3 = k2 - random.nextInt(2) - 1;
        int32_t k4 = 1 + random.nextInt(3);
        k1 = 0;

        for (int32_t l4 = l3; l4 < i && k4 > 0; --k4) {
            if (l4 >= 1) {
                const int32_t j2 = y + l4;
                i3 += TreeBlockFaces::getXOffset(face1);
                j1 += TreeBlockFaces::getZOffset(face1);

                const std::string identifier = manager.getBlockAt(i3, j2, j1).mName;
                if (identifier == "minecraft:air" || isLeaves(identifier)) {
                    _placeLogAt(manager, i3, j2, j1);
                    k1 = j2;
                }
            }

            ++l4;
        }

        if (k1 > 0) {
            int32_t branchX = i3;
            int32_t branchY = k1;
            int32_t branchZ = j1;

            for (int32_t i5 = -2; i5 <= 2; ++i5) {
                for (int32_t k5 = -2; k5 <= 2; ++k5) {
                    if (std::abs(i5) != 2 || std::abs(k5) != 2)
                        _placeLeafAt(manager, branchX + i5, branchY, branchZ + k5);
                }
            }

            branchY = branchY + 1;

            for (int32_t j5 = -1; j5 <= 1; ++j5) {
                for (int32_t l5 = -1; l5 <= 1; ++l5)
                    _placeLeafAt(manager, branchX + j5, branchY, branchZ + l5);
            }
        }
    }

    return true;
}

void SavannaTree::_placeLogAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    manager.setBlockStateAt(x, y, z, withPillarAxis(VanillaBlocks::ACACIA_LOG().toBlockState(), "y"));
}

void SavannaTree::_placeLeafAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    const std::string identifier = manager.getBlockAt(x, y, z).mName;
    if (identifier == "minecraft:air" || isLeaves(identifier))
        manager.setBlockStateAt(x, y, z, VanillaBlocks::ACACIA_LEAVES().toBlockState());
}
