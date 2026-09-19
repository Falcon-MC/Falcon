#include "Block/Blocks/LeverBlock.h"

#include "Block/BlockSupport.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"

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
    (void) player;

    RedstoneSystem::onLeverActivated(owner, position, state);
    return true;
}
