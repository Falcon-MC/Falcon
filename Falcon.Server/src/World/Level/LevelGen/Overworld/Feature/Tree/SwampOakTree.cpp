#include "World/Level/LevelGen/Overworld/Feature/Tree/SwampOakTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <chrono>
#include <cmath>

namespace {

    const int32_t SEA_LEVEL = 63;

    double ambientRandom() {
        static XoroshiroRandom random((int64_t) std::chrono::steady_clock::now().time_since_epoch().count());
        return random.nextDouble();
    }

}

SwampOakTree::SwampOakTree(int32_t minTreeHeight, int32_t maxTreeHeight)
        : mMinTreeHeight(minTreeHeight), mMaxTreeHeight(maxTreeHeight) {
}

bool SwampOakTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    int32_t height = random.nextInt(mMaxTreeHeight - mMinTreeHeight + 1) + mMinTreeHeight;

    if (y < getMinHeight() || y + height + 1 >= getMaxHeight())
        return false;

    if (!IFeature::isSupportDirt(manager.getBlockAt(x, y - 1, z)))
        return false;

    setDirtAt(manager, x, y - 1, z);

    const BlockState metaWood = withPillarAxis(VanillaBlocks::OAK_LOG().toBlockState(), "y");
    const BlockState metaLeaves = VanillaBlocks::OAK_LEAVES().toBlockState();

    for (int32_t offsetY = 0; offsetY < height; offsetY++) {
        const std::string identifier = manager.getBlockAt(x, y + offsetY, z).mName;
        if (identifier == "minecraft:air" || identifier == "minecraft:oak_leaves" || identifier == "minecraft:vine"
            || identifier == "minecraft:water") {
            manager.setBlockStateAt(x, y + offsetY, z, metaWood);
            if (identifier == "minecraft:water")
                height++;
        }
    }

    for (int32_t yy = y - 3 + height; yy <= y + height; ++yy) {
        const double yOff = (double) (yy - (y + height));
        const int32_t mid = (int32_t) (1 - yOff / 2);

        for (int32_t xx = x - mid; xx <= x + mid; ++xx) {
            const int32_t xOff = std::abs(xx - x);

            for (int32_t zz = z - mid; zz <= z + mid; ++zz) {
                const int32_t zOff = std::abs(zz - z);

                if (xOff == mid && zOff == mid && (yOff == 0 || random.nextInt(2) == 0))
                    continue;

                if (!isSolid(manager.getBlockAt(xx, yy, zz))) {
                    manager.setBlockStateAt(xx, yy, zz, metaLeaves);

                    if (random.nextInt(4) == 0)
                        _addHangingVine(manager, xx, yy, zz, _randomVineMeta(random));
                }
            }
        }
    }

    return true;
}

void SwampOakTree::_addVine(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t meta) {
    manager.setBlockStateAt(x, y, z, getVineState(meta));
}

void SwampOakTree::_addHangingVine(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t meta) {
    int32_t currentY = y - 1;
    const int32_t length = 3 + (int32_t) (ambientRandom() * 3);

    for (int32_t i = 0; i < length; i++) {
        if (manager.getBlockAt(x, currentY, z).mName == "minecraft:air" && currentY > SEA_LEVEL) {
            _addVine(manager, x, currentY, z, meta);
            currentY--;
        } else {
            break;
        }
    }
}

int32_t SwampOakTree::_randomVineMeta(IRandom &random) {
    static const int32_t METAS[] = {1, 2, 4, 8};
    return METAS[random.nextInt(3)];
}
