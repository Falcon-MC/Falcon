#pragma once

#include "World/Level/LevelGen/Nether/Tree/LegacyNetherTree.h"

class LegacyCrimsonTree : public LegacyNetherTree {
public:
    using LegacyNetherTree::LegacyNetherTree;

protected:
    BlockState getTrunkBlockState() const override;

    BlockState getLeafBlockState() const override;
};
