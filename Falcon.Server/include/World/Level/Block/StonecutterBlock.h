#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class StonecutterBlock final : public Block {
public:
    static constexpr const char *IDENTIFIER = "minecraft:stonecutter_block";

    StonecutterBlock();

    explicit StonecutterBlock(const Block &block);

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;
};
