#pragma once

#include "World/Level/LevelGen/Feature/Tree/TreeWoodType.h"
#include "World/Level/LevelGen/Feature/Tree/VanillaTreeObject.h"

class LegacyTreeObject : public VanillaTreeObject {
public:
    static void growTree(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                         TreeWoodType type, bool tall);

    virtual TreeWoodType getType() const;

    virtual int32_t getTreeHeight() const;

    virtual bool canPlaceObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random);

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

    virtual void placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random);

protected:
    static bool overridable(const BlockState &state);

    virtual void placeTrunk(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random,
                            int32_t trunkHeight);

    bool canGenerateWithVines() const;

    void setRandomTreeWithVines(IRandom &random);

    virtual BlockState getTrunkBlockState() const;

    virtual BlockState getLeafBlockState() const;

    int32_t mTreeHeight = 7;
    bool mTreeWithVines = false;
};
