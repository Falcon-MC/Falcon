#include "World/Level/LevelGen/Overworld/Feature/Tree/CherryTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <algorithm>
#include <cmath>

namespace {

    const int32_t LEAVES_RADIUS = 4;

}

CherryTree::CherryTree()
        : mLogYAxis(withPillarAxis(VanillaBlocks::CHERRY_LOG().toBlockState(), "y")),
          mLogXAxis(withPillarAxis(VanillaBlocks::CHERRY_LOG().toBlockState(), "x")),
          mLogZAxis(withPillarAxis(VanillaBlocks::CHERRY_LOG().toBlockState(), "z")),
          mLeaves(VanillaBlocks::CHERRY_LEAVES().toBlockState()) {
}

bool CherryTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const bool isBigTree = random.nextBoolean();
    if (isBigTree) {
        if (generateBigTree(manager, random, x, y, z))
            return true;
    }

    return generateSmallTree(manager, random, x, y, z);
}

bool CherryTree::generateBigTree(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const int32_t mainTrunkHeight = (random.nextBoolean() ? 1 : 0) + 10;

    if (!canPlaceObject(manager, mainTrunkHeight, x, y, z))
        return false;

    bool growOnXAxis = random.nextBoolean();
    int32_t xMultiplier = growOnXAxis ? 1 : 0;
    int32_t zMultiplier = growOnXAxis ? 0 : 1;

    const int32_t leftSideTrunkLength = random.nextInt(2, 4);
    const int32_t leftSideTrunkHeight = random.nextInt(3, 5);
    const int32_t leftSideTrunkStartY = random.nextInt(4, 5);

    if (!canPlaceObject(manager, leftSideTrunkHeight, x - leftSideTrunkLength * xMultiplier,
                        y + leftSideTrunkStartY, z - leftSideTrunkLength * zMultiplier)) {
        growOnXAxis = !growOnXAxis;
        xMultiplier = growOnXAxis ? 1 : 0;
        zMultiplier = growOnXAxis ? 0 : 1;
        if (!canPlaceObject(manager, leftSideTrunkHeight, x - leftSideTrunkLength * xMultiplier,
                            y + leftSideTrunkStartY, z - leftSideTrunkLength * zMultiplier))
            return false;
    }

    const int32_t rightSideTrunkLength = random.nextInt(2, 4);
    const int32_t rightSideTrunkHeight = random.nextInt(3, 5);
    const int32_t rightSideTrunkStartY = random.nextInt(4, 5);

    if (!canPlaceObject(manager, rightSideTrunkHeight, x + rightSideTrunkLength * xMultiplier,
                        y + rightSideTrunkStartY, z + rightSideTrunkLength * zMultiplier))
        return false;

    setDirtAt(manager, x, y - 1, z);

    for (int32_t yy = 0; yy < mainTrunkHeight; ++yy)
        manager.setBlockStateAt(x, y + yy, z, mLogYAxis);

    const BlockState &sideBlockState = growOnXAxis ? mLogXAxis : mLogZAxis;

    for (int32_t xx = 1; xx <= leftSideTrunkLength; ++xx) {
        const int32_t targetX = x - xx * xMultiplier;
        const int32_t targetZ = z - xx * zMultiplier;
        if (canGrowInto(manager.getBlockAt(targetX, y + leftSideTrunkStartY, targetZ).mName))
            manager.setBlockStateAt(targetX, y + leftSideTrunkStartY, targetZ, sideBlockState);
    }

    for (int32_t yy = 1; yy < leftSideTrunkHeight; ++yy) {
        const int32_t targetX = x - leftSideTrunkLength * xMultiplier;
        const int32_t targetZ = z - leftSideTrunkLength * zMultiplier;
        if (canGrowInto(manager.getBlockAt(targetX, y + leftSideTrunkStartY + yy, targetZ).mName))
            manager.setBlockStateAt(targetX, y + leftSideTrunkStartY + yy, targetZ, mLogYAxis);
    }

    if (leftSideTrunkStartY == 4) {
        int32_t tmpX = x - leftSideTrunkLength * xMultiplier;
        int32_t tmpY = y + leftSideTrunkStartY;
        int32_t tmpZ = z - leftSideTrunkLength * zMultiplier;
        manager.setBlockStateAt(tmpX, tmpY, tmpZ, VanillaBlocks::AIR().toBlockState());
        tmpX += xMultiplier;
        tmpY += 1;
        tmpZ += zMultiplier;
        if (canGrowInto(manager.getBlockAt(tmpX, tmpY, tmpZ).mName))
            manager.setBlockStateAt(tmpX, tmpY, tmpZ, mLogYAxis);

        tmpX -= xMultiplier;
        tmpZ -= zMultiplier;
        if (canGrowInto(manager.getBlockAt(tmpX, tmpY, tmpZ).mName))
            manager.setBlockStateAt(tmpX, tmpY, tmpZ, sideBlockState);
    }

    for (int32_t xx = 1; xx <= rightSideTrunkLength; ++xx) {
        const int32_t targetX = x + xx * xMultiplier;
        const int32_t targetZ = z + xx * zMultiplier;
        if (canGrowInto(manager.getBlockAt(targetX, y + rightSideTrunkStartY, targetZ).mName))
            manager.setBlockStateAt(targetX, y + rightSideTrunkStartY, targetZ, sideBlockState);
    }

    for (int32_t yy = 1; yy < rightSideTrunkHeight; ++yy) {
        const int32_t targetX = x + rightSideTrunkLength * xMultiplier;
        const int32_t targetZ = z + rightSideTrunkLength * zMultiplier;
        if (canGrowInto(manager.getBlockAt(targetX, y + rightSideTrunkStartY + yy, targetZ).mName))
            manager.setBlockStateAt(targetX, y + rightSideTrunkStartY + yy, targetZ, mLogYAxis);
    }

    if (rightSideTrunkStartY == 4) {
        int32_t tmpX = x + rightSideTrunkLength * xMultiplier;
        int32_t tmpY = y + rightSideTrunkStartY;
        int32_t tmpZ = z + rightSideTrunkLength * zMultiplier;
        manager.setBlockStateAt(tmpX, tmpY, tmpZ, VanillaBlocks::AIR().toBlockState());
        tmpX -= xMultiplier;
        tmpY += 1;
        tmpZ -= zMultiplier;
        if (canGrowInto(manager.getBlockAt(tmpX, tmpY, tmpZ).mName))
            manager.setBlockStateAt(tmpX, tmpY, tmpZ, mLogYAxis);

        tmpX += xMultiplier;
        tmpZ += zMultiplier;
        if (canGrowInto(manager.getBlockAt(tmpX, tmpY, tmpZ).mName))
            manager.setBlockStateAt(tmpX, tmpY, tmpZ, sideBlockState);
    }

    generateLeaves(manager, random, x, y + mainTrunkHeight + 1, z);
    generateLeaves(manager, random, x - leftSideTrunkLength * xMultiplier,
                   y + leftSideTrunkStartY + leftSideTrunkHeight + 1, z - leftSideTrunkLength * zMultiplier);
    generateLeaves(manager, random, x + rightSideTrunkLength * xMultiplier,
                   y + rightSideTrunkStartY + rightSideTrunkHeight + 1, z + rightSideTrunkLength * zMultiplier);
    return true;
}

bool CherryTree::generateSmallTree(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const int32_t mainTrunkHeight = (random.nextBoolean() ? 1 : 0) + 4;
    const int32_t sideTrunkHeight = random.nextInt(3, 5);

    if (!canPlaceObject(manager, mainTrunkHeight + 1, x, y, z))
        return false;

    int32_t growDirection = random.nextInt(0, 3);
    int32_t xMultiplier = 0;
    int32_t zMultiplier = 0;
    bool canPlace = false;

    for (int32_t i = 0; i < 4; i++) {
        growDirection = (growDirection + 1) % 4;
        if (growDirection == 0)
            xMultiplier = -1;
        else if (growDirection == 1)
            xMultiplier = 1;
        else
            xMultiplier = 0;

        if (growDirection == 2)
            zMultiplier = -1;
        else if (growDirection == 3)
            zMultiplier = 1;
        else
            zMultiplier = 0;

        if (canPlaceObject(manager, sideTrunkHeight, x + xMultiplier * sideTrunkHeight, y,
                           z + zMultiplier * sideTrunkHeight)) {
            canPlace = true;
            break;
        }
    }

    if (!canPlace)
        return false;

    const BlockState &sideBlockState = xMultiplier == 0 ? mLogZAxis : mLogXAxis;

    for (int32_t yy = 0; yy < mainTrunkHeight; ++yy) {
        if (canGrowInto(manager.getBlockAt(x, y + yy, z).mName))
            manager.setBlockStateAt(x, y + yy, z, mLogYAxis);
    }

    for (int32_t yy = 1; yy <= sideTrunkHeight; ++yy) {
        const int32_t tmpX = x + yy * xMultiplier;
        int32_t tmpY = y + mainTrunkHeight + yy - 2;
        const int32_t tmpZ = z + yy * zMultiplier;

        if (canGrowInto(manager.getBlockAt(tmpX, tmpY, tmpZ).mName))
            manager.setBlockStateAt(tmpX, tmpY, tmpZ, sideBlockState);

        if (yy == sideTrunkHeight - 1 && sideTrunkHeight > 3)
            continue;

        tmpY += 1;
        if (canGrowInto(manager.getBlockAt(tmpX, tmpY, tmpZ).mName))
            manager.setBlockStateAt(tmpX, tmpY, tmpZ, mLogYAxis);
    }

    generateLeaves(manager, random, x + sideTrunkHeight * xMultiplier, y + mainTrunkHeight + sideTrunkHeight,
                   z + sideTrunkHeight * zMultiplier);

    return true;
}

void CherryTree::generateLeaves(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    for (int32_t dy = -2; dy <= 2; dy++) {
        for (int32_t dx = -LEAVES_RADIUS; dx <= LEAVES_RADIUS; dx++) {
            for (int32_t dz = -LEAVES_RADIUS; dz <= LEAVES_RADIUS; dz++) {
                const int32_t currentRadius = LEAVES_RADIUS - std::max(1, std::abs(dy));
                if (dx * dx + dz * dz > currentRadius * currentRadius)
                    continue;

                std::string identifier = manager.getBlockAt(x + dx, y + dy, z + dz).mName;
                if (identifier == "minecraft:air" || isLeaves(identifier)
                    || identifier == "minecraft:azalea_leaves_flowered")
                    manager.setBlockStateAt(x + dx, y + dy, z + dz, mLeaves);

                if (dy == -2 && random.nextInt(0, 2) == 0) {
                    identifier = manager.getBlockAt(x + dx, y + dy - 1, z + dz).mName;
                    if (identifier == "minecraft:air" || isLeaves(identifier)
                        || identifier == "minecraft:azalea_leaves_flowered")
                        manager.setBlockStateAt(x + dx, y + dy - 1, z + dz, mLeaves);
                }
            }
        }
    }
}

bool CherryTree::canPlaceObject(BlockManager &manager, int32_t treeHeight, int32_t x, int32_t y, int32_t z) {
    int32_t radiusToCheck = 0;

    for (int32_t yy = 0; yy < treeHeight + 3; ++yy) {
        if (yy == 1 || yy == treeHeight)
            ++radiusToCheck;

        for (int32_t xx = -radiusToCheck; xx < (radiusToCheck + 1); ++xx) {
            for (int32_t zz = -radiusToCheck; zz < (radiusToCheck + 1); ++zz) {
                if (!canGrowInto(manager.getBlockAt(x + xx, y + yy, z + zz).mName))
                    return false;
            }
        }
    }

    return true;
}
