#pragma once

#include "World/Level/Block/Block.h"

class LeavesBlock : public Block {
public:
    explicit LeavesBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    PistonMoveReaction getPistonMoveReaction() const override;

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                  std::vector<BlockDrop> &drops) const override;

    // Shared with flowered azalea leaves, which have their own block class.
    static void leafDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                          std::vector<BlockDrop> &drops);
};
