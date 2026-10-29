#include "World/Level/LevelGen/Feature/Tree/HugeTreeGenerator.h"

#include "World/Level/LevelGen/Feature/BlockManager.h"

HugeTreeGenerator::HugeTreeGenerator(int32_t baseHeight, int32_t extraRandomHeight, const BlockState &woodMetadata,
                                     const BlockState &leavesMetadata)
        : mBaseHeight(baseHeight), mExtraRandomHeight(extraRandomHeight), mWoodMetadata(woodMetadata),
          mLeavesMetadata(leavesMetadata) {
}

int32_t HugeTreeGenerator::getHeight(IRandom &random) {
    int32_t height = random.nextInt(3) + mBaseHeight;

    if (mExtraRandomHeight > 1)
        height += random.nextInt(mExtraRandomHeight);

    return height;
}

bool HugeTreeGenerator::_isSpaceAt(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t height) {
    bool flag = true;

    if (y >= 1 && y + height + 1 <= 256) {
        for (int32_t i = 0; i <= 1 + height; ++i) {
            int32_t j = 2;

            if (i == 0)
                j = 1;
            else if (i >= 1 + height - 2)
                j = 2;

            for (int32_t k = -j; k <= j && flag; ++k) {
                for (int32_t l = -j; l <= j && flag; ++l) {
                    if (y + i < 0 || y + i >= 256
                        || !canGrowInto(manager.getBlockAt(x + k, y + i, z + l).mName))
                        flag = false;
                }
            }
        }

        return flag;
    }

    return false;
}

bool HugeTreeGenerator::_ensureDirtsUnderneath(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    const std::string identifier = manager.getBlockAt(x, y - 1, z).mName;

    if ((identifier == "minecraft:grass_block" || identifier == "minecraft:dirt") && y >= 2) {
        setDirtAt(manager, x, y - 1, z);
        setDirtAt(manager, x + 1, y - 1, z);
        setDirtAt(manager, x, y - 1, z + 1);
        setDirtAt(manager, x + 1, y - 1, z + 1);
        return true;
    }

    return false;
}

bool HugeTreeGenerator::ensureGrowable(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z,
                                       int32_t height) {
    (void) random;
    return _isSpaceAt(manager, x, y, z, height) && _ensureDirtsUnderneath(manager, x, y, z);
}

void HugeTreeGenerator::growLeavesLayerStrict(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t width) {
    const int32_t squared = width * width;

    for (int32_t j = -width; j <= width + 1; ++j) {
        for (int32_t k = -width; k <= width + 1; ++k) {
            const int32_t l = j - 1;
            const int32_t i1 = k - 1;

            if (j * j + k * k <= squared || l * l + i1 * i1 <= squared || j * j + i1 * i1 <= squared
                || l * l + k * k <= squared) {
                const BlockState &state = manager.getBlockAt(x + j, y, z + k);
                if (isAir(state) || isLeaves(state.mName))
                    manager.setBlockStateAt(x + j, y, z + k, mLeavesMetadata);
            }
        }
    }
}

void HugeTreeGenerator::growLeavesLayer(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t width) {
    const int32_t squared = width * width;

    for (int32_t j = -width; j <= width; ++j) {
        for (int32_t k = -width; k <= width; ++k) {
            if (j * j + k * k <= squared) {
                const BlockState &state = manager.getBlockAt(x + j, y, z + k);
                if (isAir(state) || isLeaves(state.mName))
                    manager.setBlockStateAt(x + j, y, z + k, mLeavesMetadata);
            }
        }
    }
}
