#pragma once

#include "Block/Block.h"

#include <string>

class InfestedBlock final : public Block {
public:
    explicit InfestedBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    static bool infestedStateOf(const BlockState &host, BlockState &infested);

    BlockState hostStateOf(const BlockState &state) const;

    void release(ServerNetworkHandler &owner, Level &level, const Vector3i &position) const;
};
