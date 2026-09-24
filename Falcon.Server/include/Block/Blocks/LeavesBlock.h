#pragma once

#include "Block/Block.h"

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
