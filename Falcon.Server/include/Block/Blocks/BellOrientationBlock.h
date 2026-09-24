#pragma once

#include "Block/Block.h"

#include <string>

class BellOrientationBlock final : public Block {
public:
    explicit BellOrientationBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};
