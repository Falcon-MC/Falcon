#pragma once

#include "Block/Blocks/WallAttachedBlock.h"

#include <string>

class CoralFan : public WallAttachedBlock {
public:
    explicit CoralFan(const Block &block) : WallAttachedBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;
};
