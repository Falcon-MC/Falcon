#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class ScaffoldingBlock final : public Block {
public:
    explicit ScaffoldingBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool isClimbable() const override {
        return true;
    }

    BlockTraversal getTraversal() const override {
        return BlockTraversal::Scaffolding;
    }

    // nothing from the side or from below: a full cube under a mover already above it that isn't climbing down
    bool getMoverCollisionShapes(const BlockState &state, const Vector3i &position, const AxisAlignedBB &mover,
                                 bool descending, std::vector<AxisAlignedBB> &shapes) const override;

    // the same rule for every other query made for an actor, such as whether it fits standing up
    bool getCollisionShapeAt(const BlockState &state, const Vector3i &position, const Actor *actor,
                             AxisAlignedBB &shape) const override;

    Vector3i resolvePlacementPosition(Level &level, const Vector3i &position, int blockFace) const override;

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    void onPlacing(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                   BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;
};
