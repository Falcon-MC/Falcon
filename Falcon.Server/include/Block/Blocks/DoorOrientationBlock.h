#pragma once

#include "Block/Block.h"

#include <string>
#include <vector>

class DoorOrientationBlock final : public Block {
public:
    explicit DoorOrientationBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    std::vector<BlockPlacementEntry> getPlacementBlocks(Level &level, const Vector3i &position,
                                                        const BlockState &state,
                                                        int playerFacing) const override;

    std::vector<Vector3i> getAffectedBlocks(Level &level, const Vector3i &position,
                                            const BlockState &state) const override;

    PistonMoveReaction getPistonMoveReaction() const override;
};
