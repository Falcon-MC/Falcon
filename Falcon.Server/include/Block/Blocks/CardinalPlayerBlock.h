#pragma once

#include "Block/Block.h"

#include <string>

class CardinalPlayerBlock : public Block {
public:
    explicit CardinalPlayerBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};
