#pragma once

#include "Block/Block.h"

#include <string>

class ThinFenceBlock final : public Block {
public:
    explicit ThinFenceBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const override;
};
