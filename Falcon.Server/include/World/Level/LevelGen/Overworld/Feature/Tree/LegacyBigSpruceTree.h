#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacySpruceTree.h"

class LegacyBigSpruceTree : public LegacySpruceTree {
public:
    LegacyBigSpruceTree(float leafStartHeightMultiplier, int32_t baseLeafRadius);

    void setRandomTreeHeight(IRandom &random);

    void placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) override;

    void placeLeaves(BlockManager &manager, int32_t topSize, int32_t leafRadius, int32_t x, int32_t y, int32_t z,
                     IRandom &random) override;

protected:
    void placeTrunk(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                    int32_t trunkHeight) override;

private:
    float mLeafStartHeightMultiplier;
    int32_t mBaseLeafRadius;
};
