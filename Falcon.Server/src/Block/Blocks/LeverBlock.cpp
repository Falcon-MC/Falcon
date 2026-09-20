#include "Block/Blocks/LeverBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(LeverBlock, 80);

#include "Block/BlockSupport.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

bool LeverBlock::matches(const std::string &identifier)
{
    return identifier == "minecraft:lever";
}

bool LeverBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const
{
    const Vector3i supportPosition = BlockSupport::supportOf(position, blockFace);
    const BlockState support = level.getBlockState(supportPosition.x, supportPosition.y, supportPosition.z);
    return BlockSupport::isAttachable(support, blockFace);
}

bool LeverBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                            const BlockState &state) const
{
    RedstoneSystem::onLeverActivated(owner, owner.getLevelFor(player), position, state);
    return true;
}
