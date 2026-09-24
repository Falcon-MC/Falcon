#pragma once

#include "Block/Blocks/WallAttachedBlock.h"

#include <string>

class CoralFanBlock : public WallAttachedBlock {
public:
    explicit CoralFanBlock(const Block &block) : WallAttachedBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;
};
