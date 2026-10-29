#pragma once

#include "World/Level/LevelGen/Feature/Tree/TreeBlockFace.h"
#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

class JungleTree : public VanillaTreeObject {
public:
    JungleTree(int32_t minTreeHeight, int32_t maxTreeHeight);

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    void _placeCocoa(BlockManager &manager, int32_t age, int32_t x, int32_t y, int32_t z, TreeBlockFace side);

    void _addVine(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t meta);

    void _addHangingVine(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t meta);

    static bool _isAirBlock(BlockManager &manager, int32_t x, int32_t y, int32_t z);

    int32_t mMinTreeHeight;
    int32_t mMaxTreeHeight;
};
