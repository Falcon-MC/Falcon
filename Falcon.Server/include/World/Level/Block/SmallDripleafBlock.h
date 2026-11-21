#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// two blocks tall; bone meal turns it into a big dripleaf
class SmallDripleafBlock : public Block {
public:
    explicit SmallDripleafBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                  std::vector<BlockDrop> &drops) const override;

    bool onFertilized(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
