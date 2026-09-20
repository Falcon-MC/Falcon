#pragma once

#include "Block/Block.h"

#include <string>

class DragonEggBlock final : public Block {
public:
    explicit DragonEggBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    void onTouch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                 const BlockState &state) const override;

    bool onPunch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                 const BlockState &state) const override;

private:
    static constexpr int TELEPORT_ATTEMPTS = 1000;

    static constexpr int TELEPORT_HORIZONTAL_RANGE = 16;

    static constexpr int TELEPORT_VERTICAL_RANGE = 16;

    static void teleport(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state);
};
