#pragma once

#include "Block/Block.h"

#include <string>

class SnifferEggBlock : public Block {
public:
    explicit SnifferEggBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

private:
    static void scheduleNextCrack(Level &level, const Vector3i &position);
};
