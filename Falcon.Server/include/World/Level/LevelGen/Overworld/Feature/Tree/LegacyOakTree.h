#pragma once

#include "World/Level/LevelGen/Feature/Tree/LegacyTreeObject.h"

class LegacyOakTree : public LegacyTreeObject {
public:
    TreeWoodType getType() const override;

    void placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) override;

    bool canPlaceObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) override;

private:
    bool _hasFancyBrace(BlockManager &manager, int32_t x, int32_t y, int32_t z);
};
