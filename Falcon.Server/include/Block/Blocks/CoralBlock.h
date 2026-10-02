#pragma once

#include "Block/Block.h"

#include <string>

class CoralBlock : public Block {
public:
    explicit CoralBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    static bool isLiveCoral(const std::string &identifier);

    static void scheduleDeathCheck(Level &level, const Vector3i &position);

    static void dieWithoutWater(Level &level, const Vector3i &position, const BlockState &state);

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

private:
    static bool isWater(Level &level, const Vector3i &position, int layer);

    static bool hasWater(Level &level, const Vector3i &position, const BlockState &state);
};
