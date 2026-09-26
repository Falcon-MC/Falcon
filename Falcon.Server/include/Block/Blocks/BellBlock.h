#pragma once

#include "Block/Block.h"

#include <string>

class BellBlock final : public Block {
public:
    explicit BellBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};
