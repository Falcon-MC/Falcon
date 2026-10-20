#pragma once

#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

class CherryTree : public VanillaTreeObject {
public:
    CherryTree();

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

    bool generateBigTree(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z);

    bool generateSmallTree(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z);

    void generateLeaves(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z);

    bool canPlaceObject(BlockManager &manager, int32_t treeHeight, int32_t x, int32_t y, int32_t z);

protected:
    BlockState mLogYAxis;
    BlockState mLogXAxis;
    BlockState mLogZAxis;
    BlockState mLeaves;
};
