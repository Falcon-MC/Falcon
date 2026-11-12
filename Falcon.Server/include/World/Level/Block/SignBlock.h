#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class SignBlock final : public Block {
public:
    explicit SignBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    PistonMoveReaction getPistonMoveReaction() const override;

    // a sign put against the side of a block becomes its wall block
    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, const BlockState &state,
                  const ItemStack &usedItem, int blockFace) const override;
};
