#pragma once

#include "World/Level/LevelGen/Feature/Tree/HugeTreeGenerator.h"

class JungleBigTree : public HugeTreeGenerator {
public:
    JungleBigTree(int32_t baseHeight, int32_t extraRandomHeight);

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    void _placeVine(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z, int32_t meta);

    void _createCrown(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t radius);
};
