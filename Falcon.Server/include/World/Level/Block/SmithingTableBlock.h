#pragma once

#include "World/Level/Block/Block.h"

class SmithingTableBlock final : public Block {
public:
    explicit SmithingTableBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;
};
