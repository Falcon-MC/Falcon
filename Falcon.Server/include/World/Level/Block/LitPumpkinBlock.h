#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class LitPumpkinBlock final : public Block {
public:
    explicit LitPumpkinBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    void onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;
};
