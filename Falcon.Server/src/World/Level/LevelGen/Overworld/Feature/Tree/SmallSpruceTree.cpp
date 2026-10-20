#include "World/Level/LevelGen/Overworld/Feature/Tree/SmallSpruceTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <cmath>

bool SmallSpruceTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const int32_t height = 6 + random.nextInt(4);
    const bool treeWithVines = random.nextInt(TREE_WITH_VINES_CHANCE) == 0;

    if (y < 1 || y + height + 2 >= 256)
        return false;

    const std::string ground = manager.getBlockAt(x, y - 1, z).mName;
    if (ground != "minecraft:grass_block" && ground != "minecraft:dirt" && ground != "minecraft:podzol")
        return false;

    if (!_placeTreeOfHeight(manager, x, y, z, height))
        return false;

    const int32_t trunkHeight = height - random.nextInt(3);
    for (int32_t offsetY = 0; offsetY < trunkHeight; offsetY++) {
        _placeLogAt(manager, x, y + offsetY, z);
        if (treeWithVines)
            addVinesAroundLog(manager, x, y + offsetY, z);
    }

    const int32_t topSize = height - (1 + random.nextInt(2));
    const int32_t leafRadius = 2 + random.nextInt(2);
    int32_t radius = random.nextInt(2);
    int32_t maxR = 1;
    int32_t minR = 0;

    for (int32_t yy = 0; yy <= topSize; yy++) {
        const int32_t yyy = y + height - yy;

        for (int32_t xx = x - radius; xx <= x + radius; xx++) {
            const int32_t xOff = std::abs(xx - x);

            for (int32_t zz = z - radius; zz <= z + radius; zz++) {
                const int32_t zOff = std::abs(zz - z);

                if (xOff == radius && zOff == radius && radius > 0)
                    continue;

                if (!canGrowInto(manager.getBlockAt(xx, yyy, zz).mName))
                    continue;

                _placeLeafAt(manager, xx, yyy, zz);
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

    return true;
}

bool SmallSpruceTree::_placeTreeOfHeight(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t height) {
    for (int32_t offsetY = 0; offsetY <= height + 1; ++offsetY) {
        const int32_t r = (offsetY == 0) ? 0 : (offsetY >= height - 1 ? 2 : 1);
        for (int32_t dx = -r; dx <= r; dx++) {
            for (int32_t dz = -r; dz <= r; dz++) {
                if (!canGrowInto(manager.getBlockAt(x + dx, y + offsetY, z + dz).mName))
                    return false;
            }
        }
    }

    return true;
}

void SmallSpruceTree::_placeLogAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    if (canGrowInto(manager.getBlockAt(x, y, z).mName))
        manager.setBlockStateAt(x, y, z, withPillarAxis(VanillaBlocks::SPRUCE_LOG().toBlockState(), "y"));
}

void SmallSpruceTree::_placeLeafAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    const std::string identifier = manager.getBlockAt(x, y, z).mName;
    if (identifier == "minecraft:air" || identifier == "minecraft:snow_layer" || identifier == "minecraft:vine")
        manager.setBlockStateAt(x, y, z, VanillaBlocks::SPRUCE_LEAVES().toBlockState());
}
