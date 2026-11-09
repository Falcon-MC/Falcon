#include "World/Level/Block/LeverBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockSupportRules.h"

FALCON_REGISTER_BLOCK(LeverBlock, 80);

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

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

int LeverBlock::_attachedFace(const BlockState &state)
{
    const std::string direction = state.mStates.getString("lever_direction", "down_east_west");

    int facing;
    if (direction == "down_east_west" || direction == "down_north_south")
        facing = PlacementOrientation::FACE_DOWN;
    else if (direction == "up_east_west" || direction == "up_north_south")
        facing = PlacementOrientation::FACE_UP;
    else
        facing = PlacementOrientation::faceFromName(direction);

    return facing < 0 ? PlacementOrientation::FACE_DOWN : facing;
}

bool LeverBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const
{
    if (const std::optional<bool> measured = BlockSupportRules::survivesAt(level, position, state))
        return *measured;

    return canPlaceAt(level, position, _attachedFace(state));
}

// only the block it hangs on is watched: a lever set where nothing holds it stays (sub_149C39350)
bool LeverBlock::watchesNeighbour(const Vector3i &position, const BlockState &state, const Vector3i *changed) const
{
    return changed != nullptr && *changed == BlockSupport::supportOf(position, _attachedFace(state));
}

// the survival check runs on a tick one later (sub_149C383D0)
void LeverBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state) const
{
    (void) owner;
    (void) state;
    level.scheduleUpdate(position, 1);
}

void LeverBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const BlockState &state) const
{
    Block::onNeighbourChanged(owner, level, position, state);
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
