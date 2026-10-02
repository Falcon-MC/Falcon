#pragma once

#include "Block/Block.h"

class SpreadingBlock : public Block {
public:
    explicit SpreadingBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
