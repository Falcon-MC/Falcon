#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class LodestoneBlock final : public Block {
public:
    explicit LodestoneBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;
};
