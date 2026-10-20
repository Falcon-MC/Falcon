#pragma once

#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

class JungleBush : public VanillaTreeObject {
public:
    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;
};
