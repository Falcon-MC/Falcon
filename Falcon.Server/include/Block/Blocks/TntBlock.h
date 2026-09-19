#pragma once

#include "Block/Block.h"

#include <cstdint>
#include <string>

class TntBlock final : public Block {
public:
    explicit TntBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    static void prime(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int32_t fuse);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    bool onProjectileHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state, ServerActor &projectile) const override;
};
