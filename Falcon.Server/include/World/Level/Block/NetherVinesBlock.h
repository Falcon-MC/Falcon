#pragma once

#include "World/Level/Block/Block.h"

class NetherVinesBlock : public Block {
public:
    explicit NetherVinesBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    bool isClimbable() const override {
        return true;
    }

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                  std::vector<BlockDrop> &drops) const override;
};
