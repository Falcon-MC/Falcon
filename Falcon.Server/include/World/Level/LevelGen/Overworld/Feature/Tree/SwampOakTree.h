#pragma once

#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

class SwampOakTree : public VanillaTreeObject {
public:
    SwampOakTree(int32_t minTreeHeight, int32_t maxTreeHeight);

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    void _addVine(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t meta);

    void _addHangingVine(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t meta);

    static int32_t _randomVineMeta(IRandom &random);

    int32_t mMinTreeHeight;
    int32_t mMaxTreeHeight;
};
