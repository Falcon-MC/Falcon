#pragma once

#include "Block/Block.h"

#include <string>

class CopperGolemStatueBlock final : public Block {
public:
    explicit CopperGolemStatueBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                  const BlockState &state) const override;

    bool onActorEvent(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                      const std::string &event, MobActor &source) const override;

    PistonMoveReaction getPistonMoveReaction() const override;
};
