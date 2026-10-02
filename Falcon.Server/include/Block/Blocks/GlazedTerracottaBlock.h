#pragma once

#include "Block/Block.h"

#include <string>

class GlazedTerracottaBlock final : public Block {
public:
    explicit GlazedTerracottaBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    PistonMoveReaction getPistonMoveReaction() const override;
};
