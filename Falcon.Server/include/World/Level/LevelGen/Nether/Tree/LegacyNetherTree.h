#pragma once

#include "World/Level/LevelGen/Feature/Tree/LegacyTreeObject.h"

class LegacyNetherTree : public LegacyTreeObject {
public:
    LegacyNetherTree();

    explicit LegacyNetherTree(int32_t treeHeight);

    int32_t getTreeHeight() const override;

    void placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) override;

protected:
    void placeTrunk(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                    int32_t trunkHeight) override;

private:
    static bool _checkY(BlockManager &manager, int32_t y);
};
