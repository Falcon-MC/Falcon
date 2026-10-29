#pragma once

#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

class SmallPaleOakTree : public VanillaTreeObject {
public:
    SmallPaleOakTree(int32_t minTreeHeight, int32_t maxTreeHeight);

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    int32_t mMinTreeHeight;
    int32_t mMaxTreeHeight;
};
