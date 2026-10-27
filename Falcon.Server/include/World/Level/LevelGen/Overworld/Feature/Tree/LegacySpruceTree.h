#pragma once

#include "World/Level/LevelGen/Feature/Tree/LegacyTreeObject.h"

class LegacySpruceTree : public LegacyTreeObject {
public:
    TreeWoodType getType() const override;

    void placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) override;

    virtual void placeLeaves(BlockManager &manager, int32_t topSize, int32_t leafRadius, int32_t x, int32_t y,
                             int32_t z, IRandom &random);
};
