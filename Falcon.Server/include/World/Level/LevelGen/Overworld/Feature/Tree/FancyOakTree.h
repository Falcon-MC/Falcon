#pragma once

#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

#include <vector>

class FancyOakTree : public VanillaTreeObject {
public:
    FancyOakTree();

    FancyOakTree(int32_t baseHeight, int32_t heightRandA, int32_t heightRandB);

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    struct Coordinate {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
    };

    struct FoliageCoords {
        Coordinate mPos;
        int32_t mBranchBase;
    };

    bool _makeLimb(BlockManager &manager, const Coordinate &startPos, const Coordinate &endPos, bool doPlace);

    static int32_t _getSteps(int32_t x, int32_t y, int32_t z);

    static const char *_getLogAxis(const Coordinate &startPos, const Coordinate &blockPos);

    static bool _trimBranches(int32_t height, int32_t localY);

    void _makeBranches(BlockManager &manager, int32_t height, const Coordinate &origin,
                       const std::vector<FoliageCoords> &foliageCoords);

    void _createFoliage(BlockManager &manager, const Coordinate &foliagePos);

    void _placeLeavesRow(BlockManager &manager, const Coordinate &center, int32_t radius, int32_t yOffset);

    static bool _shouldSkipLocation(int32_t dx, int32_t dz, int32_t currentRadius);

    static float _treeShape(int32_t height, int32_t y);

    bool _isFree(BlockManager &manager, const Coordinate &pos);

    void _placeLogAt(BlockManager &manager, const Coordinate &pos, const char *axis);

    void _placeLeafAt(BlockManager &manager, int32_t x, int32_t y, int32_t z);

    int32_t mBaseHeight;
    int32_t mHeightRandA;
    int32_t mHeightRandB;
};
