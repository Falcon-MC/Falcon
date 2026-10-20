#pragma once

#include "World/Level/LevelGen/Feature/Tree/TreeBlockFace.h"
#include "World/Level/LevelGen/Feature/Tree/TreeWoodType.h"
#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

#include <vector>

class FallenTree : public VanillaTreeObject {
public:
    FallenTree();

    explicit FallenTree(TreeWoodType woodType);

    FallenTree(TreeWoodType woodType, int32_t minLogLength, int32_t maxLogLength);

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    struct Coordinate {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
    };

    int32_t _sampleLogLength(IRandom &random) const;

    void _setGroundHeightForFallenLogStartPos(BlockManager &manager, Coordinate &logStartPos);

    bool _canPlaceEntireFallenLog(BlockManager &manager, int32_t logLength, const Coordinate &logStartPos,
                                  TreeBlockFace direction);

    std::vector<Coordinate> _placeFallenLog(BlockManager &manager, int32_t logLength, const Coordinate &logStartPos,
                                            TreeBlockFace direction);

    void _decorateStump(BlockManager &manager, IRandom &random, const Coordinate &stumpPos);

    void _placeStumpVine(BlockManager &manager, IRandom &random, const Coordinate &pos, int32_t meta);

    void _decorateFallenLog(BlockManager &manager, IRandom &random, const std::vector<Coordinate> &fallenLog);

    static BlockState _getMushroomState(IRandom &random);

    bool _mayPlaceOn(BlockManager &manager, const Coordinate &pos);

    bool _validFallenLogPos(BlockManager &manager, const Coordinate &pos);

    bool _isOverSolidGround(BlockManager &manager, const Coordinate &pos);

    void _placeLogBlock(BlockManager &manager, const Coordinate &pos, const char *axis);

    BlockState _getLogState(const char *axis) const;

    TreeWoodType mWoodType;
    int32_t mMinLogLength;
    int32_t mMaxLogLength;
};
