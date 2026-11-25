#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class LoomBlock final : public Block {
public:
    explicit LoomBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;
};
