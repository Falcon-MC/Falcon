#include "Level/Generator/Overworld/Feature/Tree/FancyOakTree.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Core/Math/MathConstants.h"
#include "Level/Generator/Feature/BlockManager.h"

#include <algorithm>
#include <cmath>

namespace {

    const double TRUNK_SCALE = 0.618;
    const double CLUSTER_DENSITY = 1.382;
    const double BRANCH_SLOPE = 0.381;
    const double BRANCH_LENGTH = 0.328;
    const int32_t FOLIAGE_HEIGHT = 4;
    const int32_t FOLIAGE_RADIUS = 2;
    const int32_t FOLIAGE_OFFSET = 4;

    int32_t floorToInt(double value) {
        const int32_t truncated = (int32_t) value;
        return value < (double) truncated ? truncated - 1 : truncated;
    }

}

FancyOakTree::FancyOakTree() : mBaseHeight(3), mHeightRandA(11), mHeightRandB(0) {
}

FancyOakTree::FancyOakTree(int32_t baseHeight, int32_t heightRandA, int32_t heightRandB)
        : mBaseHeight(baseHeight), mHeightRandA(heightRandA), mHeightRandB(heightRandB) {
}

bool FancyOakTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const Coordinate origin{x, y, z};
    const int32_t treeHeight = mBaseHeight + random.nextInt(mHeightRandA + 1) + random.nextInt(mHeightRandB + 1);
    const int32_t height = treeHeight + 2;

    if (origin.mY < getMinHeight() + 1 || origin.mY + height + 1 >= getMaxHeight())
        return false;

    const std::string &ground = manager.getBlockAt(origin.mX, origin.mY - 1, origin.mZ).mName;
    if (ground != "minecraft:grass_block" && ground != "minecraft:dirt")
        return false;

    const int32_t trunkHeight = floorToInt(height * TRUNK_SCALE);
    const int32_t clustersPerY = std::min(1, floorToInt(CLUSTER_DENSITY + std::pow((double) height / 13.0, 2.0)));
    const int32_t trunkTop = origin.mY + trunkHeight;
    int32_t relativeY = height - 5;

    std::vector<FoliageCoords> foliageCoords;
    foliageCoords.push_back(FoliageCoords{Coordinate{origin.mX, origin.mY + relativeY, origin.mZ}, trunkTop});

    for (; relativeY >= 0; relativeY--) {
        const float shape = _treeShape(height, relativeY);
        if (shape < 0.0f)
            continue;

        for (int32_t i = 0; i < clustersPerY; i++) {
            const double radius = shape * (random.nextFloat() + BRANCH_LENGTH);
            const double angle = random.nextFloat() * 2.0f * MathConstants::PI;
            const double offsetX = radius * std::sin(angle) + 0.5;
            const double offsetZ = radius * std::cos(angle) + 0.5;
            const Coordinate checkStart{origin.mX + floorToInt(offsetX), origin.mY + relativeY - 1,
                                        origin.mZ + floorToInt(offsetZ)};
            const Coordinate checkEnd{checkStart.mX, checkStart.mY + 5, checkStart.mZ};

            if (_makeLimb(manager, checkStart, checkEnd, false)) {
                const int32_t dx = origin.mX - checkStart.mX;
                const int32_t dz = origin.mZ - checkStart.mZ;
                const double branchHeight =
                        checkStart.mY - std::sqrt((double) (dx * dx + dz * dz)) * BRANCH_SLOPE;
                const int32_t branchTop = branchHeight > trunkTop ? trunkTop : (int32_t) branchHeight;
                const Coordinate checkBranchBase{origin.mX, branchTop, origin.mZ};
                if (_makeLimb(manager, checkBranchBase, checkStart, false))
                    foliageCoords.push_back(FoliageCoords{checkStart, checkBranchBase.mY});
            }
        }
    }

    setDirtAt(manager, origin.mX, origin.mY - 1, origin.mZ);
    _makeLimb(manager, origin, Coordinate{origin.mX, origin.mY + trunkHeight, origin.mZ}, true);
    _makeBranches(manager, height, origin, foliageCoords);

    for (const FoliageCoords &foliageCoord: foliageCoords) {
        if (_trimBranches(height, foliageCoord.mBranchBase - origin.mY))
            _createFoliage(manager, foliageCoord.mPos);
    }

    return true;
}

bool FancyOakTree::_makeLimb(BlockManager &manager, const Coordinate &startPos, const Coordinate &endPos,
                             bool doPlace) {
    if (!doPlace && startPos.mX == endPos.mX && startPos.mY == endPos.mY && startPos.mZ == endPos.mZ)
        return true;

    const int32_t deltaX = endPos.mX - startPos.mX;
    const int32_t deltaY = endPos.mY - startPos.mY;
    const int32_t deltaZ = endPos.mZ - startPos.mZ;
    const int32_t steps = _getSteps(deltaX, deltaY, deltaZ);
    if (steps == 0)
        return true;

    const float stepX = (float) deltaX / (float) steps;
    const float stepY = (float) deltaY / (float) steps;
    const float stepZ = (float) deltaZ / (float) steps;

    for (int32_t i = 0; i <= steps; i++) {
        const Coordinate blockPos{
                startPos.mX + floorToInt(0.5f + i * stepX),
                startPos.mY + floorToInt(0.5f + i * stepY),
                startPos.mZ + floorToInt(0.5f + i * stepZ)
        };

        if (doPlace)
            _placeLogAt(manager, blockPos, _getLogAxis(startPos, blockPos));
        else if (!_isFree(manager, blockPos))
            return false;
    }

    return true;
}

int32_t FancyOakTree::_getSteps(int32_t x, int32_t y, int32_t z) {
    return std::max(std::abs(x), std::max(std::abs(y), std::abs(z)));
}

const char *FancyOakTree::_getLogAxis(const Coordinate &startPos, const Coordinate &blockPos) {
    const int32_t xdiff = std::abs(blockPos.mX - startPos.mX);
    const int32_t zdiff = std::abs(blockPos.mZ - startPos.mZ);
    const int32_t maxdiff = std::max(xdiff, zdiff);
    if (maxdiff > 0)
        return xdiff == maxdiff ? "x" : "z";

    return "y";
}

bool FancyOakTree::_trimBranches(int32_t height, int32_t localY) {
    return localY >= height * 0.2;
}

void FancyOakTree::_makeBranches(BlockManager &manager, int32_t height, const Coordinate &origin,
                                 const std::vector<FoliageCoords> &foliageCoords) {
    for (const FoliageCoords &endCoord: foliageCoords) {
        const int32_t branchBase = endCoord.mBranchBase;
        const Coordinate baseCoord{origin.mX, branchBase, origin.mZ};
        const bool same = baseCoord.mX == endCoord.mPos.mX && baseCoord.mY == endCoord.mPos.mY
                          && baseCoord.mZ == endCoord.mPos.mZ;
        if (!same && _trimBranches(height, branchBase - origin.mY))
            _makeLimb(manager, baseCoord, endCoord.mPos, true);
    }
}

void FancyOakTree::_createFoliage(BlockManager &manager, const Coordinate &foliagePos) {
    for (int32_t yo = FOLIAGE_OFFSET; yo >= FOLIAGE_OFFSET - FOLIAGE_HEIGHT; yo--) {
        const int32_t currentRadius =
                FOLIAGE_RADIUS + (yo != FOLIAGE_OFFSET && yo != FOLIAGE_OFFSET - FOLIAGE_HEIGHT ? 1 : 0);
        _placeLeavesRow(manager, foliagePos, currentRadius, yo);
    }
}

void FancyOakTree::_placeLeavesRow(BlockManager &manager, const Coordinate &center, int32_t radius, int32_t yOffset) {
    const int32_t y = center.mY + yOffset;
    for (int32_t dx = -radius; dx <= radius; dx++) {
        for (int32_t dz = -radius; dz <= radius; dz++) {
            if (!_shouldSkipLocation(dx, dz, radius))
                _placeLeafAt(manager, center.mX + dx, y, center.mZ + dz);
        }
    }
}

bool FancyOakTree::_shouldSkipLocation(int32_t dx, int32_t dz, int32_t currentRadius) {
    const float offsetX = (float) dx + 0.5f;
    const float offsetZ = (float) dz + 0.5f;
    return offsetX * offsetX + offsetZ * offsetZ > (float) (currentRadius * currentRadius);
}

float FancyOakTree::_treeShape(int32_t height, int32_t y) {
    if ((float) y < (float) height * 0.3f)
        return -1.0f;

    const float radius = (float) height / 2.0f;
    const float adjacent = radius - (float) y;
    float distance = (float) std::sqrt((double) (radius * radius - adjacent * adjacent));
    if (adjacent == 0.0f)
        distance = radius;
    else if (std::fabs(adjacent) >= radius)
        return 0.0f;

    return distance * 0.5f;
}

bool FancyOakTree::_isFree(BlockManager &manager, const Coordinate &pos) {
    return canGrowInto(manager.getBlockAt(pos.mX, pos.mY, pos.mZ).mName);
}

void FancyOakTree::_placeLogAt(BlockManager &manager, const Coordinate &pos, const char *axis) {
    if (_isFree(manager, pos))
        manager.setBlockStateAt(pos.mX, pos.mY, pos.mZ,
                                withPillarAxis(VanillaBlocks::OAK_LOG().toBlockState(), axis));
}

void FancyOakTree::_placeLeafAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    const BlockState &state = manager.getBlockAt(x, y, z);
    if (isAir(state) || isLeaves(state.mName))
        manager.setBlockStateAt(x, y, z, VanillaBlocks::OAK_LEAVES().toBlockState());
}
