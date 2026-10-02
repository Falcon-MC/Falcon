#pragma once

#include "Block/Block.h"

#include <string>

class EndPortalFrameBlock final : public Block {
public:
    explicit EndPortalFrameBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool tryCompletePortal(Level &level, const Vector3i &framePosition, ServerNetworkHandler *owner);
};
