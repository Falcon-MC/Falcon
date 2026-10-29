#pragma once

#include "World/Level/Block/WallAttachedBlock.h"

#include <string>

class WallSignBlock final : public WallAttachedBlock {
public:
    explicit WallSignBlock(const Block &block) : WallAttachedBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    PistonMoveReaction getPistonMoveReaction() const override;
};
