#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// suspicious sand and gravel: the block side of brushing (BrushableBlock, 26.60)
class BrushableBlock : public Block {
public:
    explicit BrushableBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, const BlockState &state,
                  const ItemStack &usedItem, int blockFace) const override;

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;
};
