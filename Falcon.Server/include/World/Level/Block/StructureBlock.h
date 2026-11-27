#pragma once

#include "World/Level/Block/Block.h"

class StructureBlock final : public Block {
public:
    explicit StructureBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    bool resistsWither(bool charging) const override {
        return true;
    }

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    void onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;
};
