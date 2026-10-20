#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <algorithm>

namespace {

    const int32_t FALLEN_LOG_MAX_GROUND_GAP = 2;
    const int32_t MAX_MUSHROOMS = 2;

    const char *axisOf(TreeBlockFace face) {
        switch (face) {
            case TreeBlockFace::WEST:
            case TreeBlockFace::EAST:
                return "x";
            case TreeBlockFace::NORTH:
            case TreeBlockFace::SOUTH:
                return "z";
            default:
                return "y";
        }
    }

}

FallenTree::FallenTree() : mWoodType(TreeWoodType::OAK), mMinLogLength(3), mMaxLogLength(7) {
}

FallenTree::FallenTree(TreeWoodType woodType) : mWoodType(woodType), mMinLogLength(3), mMaxLogLength(7) {
}

FallenTree::FallenTree(TreeWoodType woodType, int32_t minLogLength, int32_t maxLogLength)
        : mWoodType(woodType), mMinLogLength(minLogLength), mMaxLogLength(maxLogLength) {
}

bool FallenTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const Coordinate origin{x, y, z};
    if (!_mayPlaceOn(manager, origin))
        return false;

    _placeLogBlock(manager, origin, "y");
    _decorateStump(manager, random, origin);

    const TreeBlockFace direction = TreeBlockFaces::HORIZONTAL_PLANE[random.nextInt(4)];
    const int32_t logLength = _sampleLogLength(random);
    const int32_t distance = 2 + random.nextInt(2);
    Coordinate logStartPos{
            origin.mX + TreeBlockFaces::getXOffset(direction) * distance,
            origin.mY + TreeBlockFaces::getYOffset(direction) * distance,
            origin.mZ + TreeBlockFaces::getZOffset(direction) * distance
    };
    _setGroundHeightForFallenLogStartPos(manager, logStartPos);

    if (_canPlaceEntireFallenLog(manager, logLength, logStartPos, direction)) {
        const std::vector<Coordinate> fallenLog = _placeFallenLog(manager, logLength, logStartPos, direction);
        _decorateFallenLog(manager, random, fallenLog);
    }

    return true;
}

int32_t FallenTree::_sampleLogLength(IRandom &random) const {
    if (mMaxLogLength <= mMinLogLength)
        return std::max(1, mMinLogLength);

    return mMinLogLength + random.nextInt(mMaxLogLength - mMinLogLength + 1);
}

void FallenTree::_setGroundHeightForFallenLogStartPos(BlockManager &manager, Coordinate &logStartPos) {
    logStartPos.mY++;

    for (int32_t i = 0; i < 6; i++) {
        if (_mayPlaceOn(manager, logStartPos))
            return;

        logStartPos.mY--;
    }
}

bool FallenTree::_canPlaceEntireFallenLog(BlockManager &manager, int32_t logLength, const Coordinate &logStartPos,
                                          TreeBlockFace direction) {
    int32_t gapInGround = 0;
    Coordinate current = logStartPos;

    for (int32_t i = 0; i < logLength; i++) {
        if (!_validFallenLogPos(manager, current))
            return false;

        if (!_isOverSolidGround(manager, current)) {
            if (++gapInGround > FALLEN_LOG_MAX_GROUND_GAP)
                return false;
        } else {
            gapInGround = 0;
        }

        current.mX += TreeBlockFaces::getXOffset(direction);
        current.mY += TreeBlockFaces::getYOffset(direction);
        current.mZ += TreeBlockFaces::getZOffset(direction);
    }

    return true;
}

std::vector<FallenTree::Coordinate> FallenTree::_placeFallenLog(BlockManager &manager, int32_t logLength,
                                                                const Coordinate &logStartPos,
                                                                TreeBlockFace direction) {
    std::vector<Coordinate> fallenLog;
    Coordinate current = logStartPos;

    for (int32_t i = 0; i < logLength; i++) {
        _placeLogBlock(manager, current, axisOf(direction));
        fallenLog.push_back(current);
        current.mX += TreeBlockFaces::getXOffset(direction);
        current.mY += TreeBlockFaces::getYOffset(direction);
        current.mZ += TreeBlockFaces::getZOffset(direction);
    }

    return fallenLog;
}

void FallenTree::_decorateStump(BlockManager &manager, IRandom &random, const Coordinate &stumpPos) {
    if (mWoodType != TreeWoodType::OAK && mWoodType != TreeWoodType::JUNGLE)
        return;

    _placeStumpVine(manager, random, Coordinate{stumpPos.mX - 1, stumpPos.mY, stumpPos.mZ}, 8);
    _placeStumpVine(manager, random, Coordinate{stumpPos.mX + 1, stumpPos.mY, stumpPos.mZ}, 2);
    _placeStumpVine(manager, random, Coordinate{stumpPos.mX, stumpPos.mY, stumpPos.mZ - 1}, 1);
    _placeStumpVine(manager, random, Coordinate{stumpPos.mX, stumpPos.mY, stumpPos.mZ + 1}, 4);
}

void FallenTree::_placeStumpVine(BlockManager &manager, IRandom &random, const Coordinate &pos, int32_t meta) {
    if (random.nextInt(4) == 0 || !isAir(manager.getBlockAt(pos.mX, pos.mY, pos.mZ)))
        return;

    manager.setBlockStateAt(pos.mX, pos.mY, pos.mZ, getVineState(meta));
}

void FallenTree::_decorateFallenLog(BlockManager &manager, IRandom &random,
                                    const std::vector<Coordinate> &fallenLog) {
    if (fallenLog.empty() || random.nextInt(4) != 0)
        return;

    const int32_t mushrooms = 1 + random.nextInt(MAX_MUSHROOMS);
    for (int32_t i = 0; i < mushrooms; i++) {
        const Coordinate &logPos = fallenLog[(size_t) random.nextInt((int32_t) fallenLog.size())];
        const int32_t mushroomY = logPos.mY + 1;
        if (isAir(manager.getBlockAt(logPos.mX, mushroomY, logPos.mZ)))
            manager.setBlockStateAt(logPos.mX, mushroomY, logPos.mZ, _getMushroomState(random));
    }
}

BlockState FallenTree::_getMushroomState(IRandom &random) {
    return random.nextBoolean() ? VanillaBlocks::RED_MUSHROOM().toBlockState()
                                : VanillaBlocks::BROWN_MUSHROOM().toBlockState();
}

bool FallenTree::_mayPlaceOn(BlockManager &manager, const Coordinate &pos) {
    return _validFallenLogPos(manager, pos) && _isOverSolidGround(manager, pos);
}

bool FallenTree::_validFallenLogPos(BlockManager &manager, const Coordinate &pos) {
    if (pos.mY < getMinHeight() || pos.mY >= getMaxHeight())
        return false;

    return !isSolid(manager.getBlockAt(pos.mX, pos.mY, pos.mZ));
}

bool FallenTree::_isOverSolidGround(BlockManager &manager, const Coordinate &pos) {
    return isSolid(manager.getBlockAt(pos.mX, pos.mY - 1, pos.mZ));
}

void FallenTree::_placeLogBlock(BlockManager &manager, const Coordinate &pos, const char *axis) {
    manager.setBlockStateAt(pos.mX, pos.mY, pos.mZ, _getLogState(axis));
}

BlockState FallenTree::_getLogState(const char *axis) const {
    switch (mWoodType) {
        case TreeWoodType::ACACIA:
            return withPillarAxis(VanillaBlocks::ACACIA_LOG().toBlockState(), axis);
        case TreeWoodType::BIRCH:
            return withPillarAxis(VanillaBlocks::BIRCH_LOG().toBlockState(), axis);
        case TreeWoodType::CHERRY:
            return withPillarAxis(VanillaBlocks::CHERRY_LOG().toBlockState(), axis);
        case TreeWoodType::DARK_OAK:
            return withPillarAxis(VanillaBlocks::DARK_OAK_LOG().toBlockState(), axis);
        case TreeWoodType::JUNGLE:
            return withPillarAxis(VanillaBlocks::JUNGLE_LOG().toBlockState(), axis);
        case TreeWoodType::MANGROVE:
            return withPillarAxis(VanillaBlocks::MANGROVE_LOG().toBlockState(), axis);
        case TreeWoodType::PALE_OAK:
            return withPillarAxis(VanillaBlocks::PALE_OAK_LOG().toBlockState(), axis);
        case TreeWoodType::SPRUCE:
            return withPillarAxis(VanillaBlocks::SPRUCE_LOG().toBlockState(), axis);
        default:
            return withPillarAxis(VanillaBlocks::OAK_LOG().toBlockState(), axis);
    }
}
