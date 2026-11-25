#pragma once

#include "World/Level/Block/Block.h"

class CocoaBlock : public Block {
public:
    explicit CocoaBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortune,
                  std::vector<BlockDrop> &drops) const override;

    bool canPlaceAt(Level &level, const Vector3i &position, int face) const override;
    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }
};
