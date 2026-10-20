#include "World/Level/LevelGen/Nether/Tree/LegacyNetherTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"
#include "World/Level/Level.h"

#include <chrono>
#include <cmath>

LegacyNetherTree::LegacyNetherTree() {
    XoroshiroRandom random((int64_t) std::chrono::steady_clock::now().time_since_epoch().count());
    mTreeHeight = random.nextInt(9) + 4;
}

LegacyNetherTree::LegacyNetherTree(int32_t treeHeight) {
    mTreeHeight = treeHeight;
}

int32_t LegacyNetherTree::getTreeHeight() const {
    return mTreeHeight;
}

bool LegacyNetherTree::_checkY(BlockManager &manager, int32_t y) {
    const int dimension = manager.getLevel().getDimensionId();
    if (dimension == 1)
        return y > 126;

    if (dimension == 0)
        return y > 318;

    if (dimension == 2)
        return y > 254;

    return false;
}

void LegacyNetherTree::placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    if (_checkY(manager, y))
        return;

    placeTrunk(manager, x, y, z, random, getTreeHeight());

    const double blankArea = -3;
    const int32_t mid = (int32_t) (1 - blankArea / 2);

    for (int32_t yy = y - 3 + mTreeHeight; yy <= y + mTreeHeight - 1; ++yy) {
        if (_checkY(manager, yy))
            continue;

        for (int32_t xx = x - mid; xx <= x + mid; xx++) {
            const int32_t xOff = std::abs(xx - x);

            for (int32_t zz = z - mid; zz <= z + mid; zz += mid * 2) {
                const int32_t zOff = std::abs(zz - z);
                if (xOff == mid && zOff == mid && random.nextInt(2) == 0)
                    continue;

                if (!isSolid(manager.getBlockAt(xx, yy, zz))) {
                    if (random.nextInt(20) == 0)
                        manager.setBlockStateAt(xx, yy, zz, VanillaBlocks::SHROOMLIGHT().toBlockState());
                    else
                        manager.setBlockStateAt(xx, yy, zz, getLeafBlockState());
                }
            }
        }

        for (int32_t zz = z - mid; zz <= z + mid; zz++) {
            const int32_t zOff = std::abs(zz - z);

            for (int32_t xx = x - mid; xx <= x + mid; xx += mid * 2) {
                const int32_t xOff = std::abs(xx - x);
                if (xOff == mid && zOff == mid && random.nextInt(2) == 0)
                    continue;

                if (!isSolid(manager.getBlockAt(xx, yy, zz))) {
                    if (random.nextInt(20) == 0)
                        manager.setBlockStateAt(xx, yy, zz, VanillaBlocks::SHROOMLIGHT().toBlockState());
                    else
                        manager.setBlockStateAt(xx, yy, zz, getLeafBlockState());
                }
            }
        }
    }

    for (int32_t yy = y - 4 + mTreeHeight; yy <= y + mTreeHeight - 3; ++yy) {
        if (_checkY(manager, yy))
            continue;

        for (int32_t xx = x - mid; xx <= x + mid; xx++) {
            for (int32_t zz = z - mid; zz <= z + mid; zz += mid * 2) {
                if (!isSolid(manager.getBlockAt(xx, yy, zz))) {
                    if (random.nextInt(3) == 0) {
                        const int32_t limit = random.nextInt(5);
                        for (int32_t i = 0; i < limit; i++) {
                            if (!isSolid(manager.getBlockAt(xx, yy - i, zz)))
                                manager.setBlockStateAt(xx, yy - i, zz, getLeafBlockState());
                        }
                    }
                }
            }
        }

        for (int32_t zz = z - mid; zz <= z + mid; zz++) {
            for (int32_t xx = x - mid; xx <= x + mid; xx += mid * 2) {
                if (!isSolid(manager.getBlockAt(xx, yy, zz))) {
                    if (random.nextInt(3) == 0) {
                        const int32_t limit = random.nextInt(4);
                        for (int32_t i = 0; i < limit; i++) {
                            if (!isSolid(manager.getBlockAt(xx, yy - i, zz)))
                                manager.setBlockStateAt(xx, yy - i, zz, getLeafBlockState());
                        }
                    }
                }
            }
        }
    }

    for (int32_t xCanopy = x - mid + 1; xCanopy <= x + mid - 1; xCanopy++) {
        for (int32_t zCanopy = z - mid + 1; zCanopy <= z + mid - 1; zCanopy++) {
            if (!isSolid(manager.getBlockAt(xCanopy, y + mTreeHeight, zCanopy)))
                manager.setBlockStateAt(xCanopy, y + mTreeHeight, zCanopy, getLeafBlockState());
        }
    }
}

void LegacyNetherTree::placeTrunk(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                                  int32_t trunkHeight) {
    (void) random;

    manager.setBlockStateAt(x, y, z, getTrunkBlockState());

    for (int32_t yy = 0; yy < trunkHeight; ++yy) {
        if (_checkY(manager, y + yy))
            continue;

        if (overridable(manager.getBlockAt(x, y + yy, z)))
            manager.setBlockStateAt(x, y + yy, z, getTrunkBlockState());
    }
}
