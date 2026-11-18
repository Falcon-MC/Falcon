#pragma once

#include "World/Level/Block/CauldronBlock.h"
#include "World/Level/Block/SpeleothemBlock.h"

class PointedDripstoneBlock : public SpeleothemBlock {
public:
    explicit PointedDripstoneBlock(const Block &block) : SpeleothemBlock(block) {}

    static constexpr const char *IDENTIFIER = "minecraft:pointed_dripstone";

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }

    bool getCollisionShapeAt(const BlockState &state, const Vector3i &position, const Actor *actor,
                             AxisAlignedBB &shape) const override;

    // the tip of a stalactite a drop can fall from: not under water
    static bool isStalactiteTip(Level &level, const Vector3i &position);

    // the source on top of the block the stalactite hangs from, water or lava
    static bool getDripFluid(Level &level, const Vector3i &stalactite, CauldronBlock::Liquid &fluid);

private:
    void growFromRoot(Level &level, const Vector3i &root) const;

    void growStalagmiteBelow(Level &level, const Vector3i &tip) const;

    // the drop's particle, and the cauldron below it fills once the drop lands
    void dripIntoCauldron(ServerNetworkHandler &owner, Level &level, const Vector3i &root,
                          CauldronBlock::Liquid fluid) const;
};
