#pragma once

#include "Block/Blocks/CardinalPlayerBlock.h"

#include <string>

class FenceGateBlock final : public CardinalPlayerBlock {
public:
    explicit FenceGateBlock(const Block &block) : CardinalPlayerBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;
};
