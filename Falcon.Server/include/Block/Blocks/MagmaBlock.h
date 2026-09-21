#pragma once

#include "Block/Block.h"

#include <string>

class MagmaBlock final : public Block {
public:
    explicit MagmaBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onStepOn(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state) const override;
};
