#pragma once

#include "World/Level/LevelGen/Feature/Tree/LegacyTreeObject.h"

class LegacyDarkOakTree : public LegacyTreeObject {
public:
    LegacyDarkOakTree(float leafStartHeightMultiplier, int32_t baseLeafRadius);

    TreeWoodType getType() const override;

    void setRandomTreeHeight(IRandom &random);

    void placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) override;

    void placeLeaves(BlockManager &manager, int32_t topSize, int32_t leafRadius, int32_t x, int32_t y, int32_t z,
                     IRandom &random);

protected:
    void placeTrunk(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                    int32_t trunkHeight) override;

private:
    float mLeafStartHeightMultiplier;
    int32_t mBaseLeafRadius;
};
