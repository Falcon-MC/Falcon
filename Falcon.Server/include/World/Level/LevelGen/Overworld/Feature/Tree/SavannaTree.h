#pragma once

#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

class SavannaTree : public VanillaTreeObject {
public:
    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    void _placeLogAt(BlockManager &manager, int32_t x, int32_t y, int32_t z);

    void _placeLeafAt(BlockManager &manager, int32_t x, int32_t y, int32_t z);
};
