#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class FacingMachineBlock : public Block {
public:
    explicit FacingMachineBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};
