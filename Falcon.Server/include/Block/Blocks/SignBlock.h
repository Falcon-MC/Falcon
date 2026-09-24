#pragma once

#include "Block/Block.h"

#include <string>

class SignBlock final : public Block {
public:
    explicit SignBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    PistonMoveReaction getPistonMoveReaction() const override;
};
