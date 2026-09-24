#pragma once

#include "Block/Blocks/CardinalPlayerBlock.h"

#include <string>
#include <vector>

class BedOrientationBlock final : public CardinalPlayerBlock {
public:
    explicit BedOrientationBlock(const Block &block) : CardinalPlayerBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    std::vector<BlockPlacementEntry> getPlacementBlocks(Level &level, const Vector3i &position,
                                                        const BlockState &state,
                                                        int playerFacing) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                  const BlockState &state) const override;

    std::vector<Vector3i> getAffectedBlocks(Level &level, const Vector3i &position,
                                            const BlockState &state) const override;
};
