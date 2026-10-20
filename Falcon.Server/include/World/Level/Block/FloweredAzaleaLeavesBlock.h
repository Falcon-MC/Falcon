#pragma once

#include "World/Level/Block/Block.h"

class FloweredAzaleaLeavesBlock final : public Block {
public:
    explicit FloweredAzaleaLeavesBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    PistonMoveReaction getPistonMoveReaction() const override;
};
