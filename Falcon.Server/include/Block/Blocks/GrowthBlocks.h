#pragma once

#include "Block/Blocks/PlantBlock.h"
#include "Level/Generator/Feature/Tree/TreeWoodType.h"

class CropBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

protected:
    virtual const char *getGrowthState() const { return "growth"; }

    virtual int getMaxGrowth() const { return 7; }

    virtual int getGrowthChance() const { return 2; }

    virtual bool needsLight() const { return true; }
};

class NetherWartBlock : public CropBlock {
public:
    using CropBlock::CropBlock;

    static bool matches(const std::string &identifier);

protected:
    const char *getGrowthState() const override { return "age"; }

    int getMaxGrowth() const override { return 3; }

    int getGrowthChance() const override { return 10; }

    bool needsLight() const override { return false; }
};

class StemBlock : public CropBlock {
public:
    using CropBlock::CropBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

private:
    BlockState getFruitState() const;
};

class SaplingBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    bool growTree(ServerNetworkHandler &owner, Level &level, const Vector3i &position) const;

    TreeWoodType getWoodType() const;
};

class LeavesBlock : public Block {
public:
    explicit LeavesBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    PistonMoveReaction getPistonMoveReaction() const override;
};

class FloweredAzaleaLeavesBlock final : public Block {
public:
    explicit FloweredAzaleaLeavesBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    PistonMoveReaction getPistonMoveReaction() const override;
};

class SpreadingBlock : public Block {
public:
    explicit SpreadingBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};

class NyliumBlock : public Block {
public:
    explicit NyliumBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
