#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class BellBlock final : public Block {
public:
    explicit BellBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    // BellBlock::use (sub_14315C7C0): rings when the player faces the right way for its attachment
    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, const BlockState &state,
                  const ItemStack &usedItem, int blockFace) const override;
};
