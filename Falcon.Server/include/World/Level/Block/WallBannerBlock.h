#pragma once

#include "World/Level/Block/WallAttachedBlock.h"

#include <string>

class WallBannerBlock final : public WallAttachedBlock {
public:
    explicit WallBannerBlock(const Block &block) : WallAttachedBlock(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, const BlockState &state,
                  const ItemStack &usedItem, int blockFace) const override;
};
