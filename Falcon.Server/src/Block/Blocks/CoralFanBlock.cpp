#include "Block/Blocks/CoralFanBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/CoralBlock.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(CoralFanBlock, 185);

bool CoralFanBlock::matches(const std::string &identifier) {
    return CoralBlock::isLiveCoral(identifier) && !CoralBlock::matches(identifier);
}

void CoralFanBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) const {
    (void) owner;

    CoralBlock::dieWithoutWater(level, position, state);
}

void CoralFanBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    WallAttachedBlock::onNeighbourChanged(owner, level, position, state);

    if (level.getBlockState(position.x, position.y, position.z).mName == state.mName)
        CoralBlock::scheduleDeathCheck(level, position);
}
