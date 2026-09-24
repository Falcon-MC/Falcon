#pragma once

#include "Block/Block.h"

#include <string>

class WallAttachedBlock : public Block {
public:
    explicit WallAttachedBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};
