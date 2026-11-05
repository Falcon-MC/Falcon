#pragma once

#include "World/Level/Block/Block.h"

class FloweredAzaleaLeavesBlock final : public Block {
public:
    explicit FloweredAzaleaLeavesBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool isLeaves() const override {
        return true;
    }

    PistonMoveReaction getPistonMoveReaction() const override;

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                  std::vector<BlockDrop> &drops) const override;
};
