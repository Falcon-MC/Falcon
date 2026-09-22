#include "Block/Blocks/LeverBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(LeverBlock, 80);

#include "Block/BlockSupport.h"
#include "Block/Components/PlacementOrientation.h"
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

bool LeverBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const
{
    const std::string direction = state.mStates.getString("lever_direction", "down_east_west");

    int facing;
    if (direction == "down_east_west" || direction == "down_north_south")
        facing = PlacementOrientation::FACE_DOWN;
    else if (direction == "up_east_west" || direction == "up_north_south")
        facing = PlacementOrientation::FACE_UP;
    else
        facing = PlacementOrientation::faceFromName(direction);

    if (facing < 0)
        facing = PlacementOrientation::FACE_DOWN;

    return canPlaceAt(level, position, facing);
}

bool LeverBlock::isSignalSource() const
{
    return true;
}

bool LeverBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                            const BlockState &state) const
{
    RedstoneSystem::onLeverActivated(owner, owner.getLevelFor(player), position, state);
    return true;
}
