#include "Block/Blocks/CoralFanBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/CoralBlock.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

FALCON_REGISTER_BLOCK(CoralFanBlock, 185);

bool CoralFanBlock::matches(const std::string &identifier) {
    return CoralBlock::isLiveCoral(identifier) && !CoralBlock::matches(identifier);
}

void CoralFanBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) const {
    (void) owner;

    CoralBlock::dieWithoutWater(level, position, state);
}

void CoralFanBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                             const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    WallAttachedBlock::onPlaced(owner, player, position, state, usedItem, blockFace);

    CoralBlock::scheduleDeathCheck(owner.getLevelFor(player), position);
}

void CoralFanBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    WallAttachedBlock::onNeighbourChanged(owner, level, position, state);

    if (level.getBlockState(position.x, position.y, position.z).mName == state.mName)
        CoralBlock::scheduleDeathCheck(level, position);
}
