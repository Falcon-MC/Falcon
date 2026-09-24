#pragma once

#include "Block/Block.h"

#include <string>

class WallBlock final : public Block {
public:
    explicit WallBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const override;
};
