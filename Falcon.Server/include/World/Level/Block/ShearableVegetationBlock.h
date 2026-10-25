#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// Vegetation without other behaviour whose only drop is itself, when cut with shears or Silk Touch.
class ShearableVegetationBlock : public Block {
public:
    explicit ShearableVegetationBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                  std::vector<BlockDrop> &drops) const override;
};
