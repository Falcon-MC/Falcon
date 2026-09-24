#pragma once

#include "Block/Blocks/RedstoneDiodeBlock.h"

#include <string>

class RedstoneRepeaterBlock final : public RedstoneDiodeBlock {
public:
    explicit RedstoneRepeaterBlock(const Block &block) : RedstoneDiodeBlock(block)
    {
    }

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool matches(const std::string &identifier);

    bool isPowered(const BlockState &state) const override;

    BlockState getPoweredState(const BlockState &state) const override;

    BlockState getUnpoweredState(const BlockState &state) const override;
};
