#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleBush.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

bool JungleBush::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    (void) random;

    manager.setBlockStateAt(x, y, z, withPillarAxis(VanillaBlocks::JUNGLE_LOG().toBlockState(), "y"));

    for (int32_t offsetY = -2; offsetY <= 1; offsetY++) {
        const int32_t radius = 2 - offsetY;

        for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
                if (offsetX * offsetX + offsetZ * offsetZ > radius * radius)
                    continue;

                const BlockState &existing = manager.getBlockAt(x + offsetX, y + offsetY, z + offsetZ);
                if (isAir(existing) || canBeReplaced(existing) || isLeaves(existing.mName))
                    manager.setBlockStateAt(x + offsetX, y + offsetY, z + offsetZ,
                                            VanillaBlocks::JUNGLE_LEAVES().toBlockState());
            }
        }
    }

    return true;
}
