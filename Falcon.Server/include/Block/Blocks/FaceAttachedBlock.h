#pragma once

#include "Block/Block.h"

#include <string>

class FaceAttachedBlock : public Block {
public:
    explicit FaceAttachedBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};
