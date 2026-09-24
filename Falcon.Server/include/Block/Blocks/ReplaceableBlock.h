#pragma once

#include "Block/Block.h"

#include <string>

class ReplaceableBlock : public Block {
public:
    explicit ReplaceableBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canBeReplaced(const BlockState &state) const override;

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                  std::vector<BlockDrop> &drops) const override;
};
