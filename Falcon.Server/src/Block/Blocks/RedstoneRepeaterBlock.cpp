#include "Block/Blocks/RedstoneRepeaterBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(RedstoneRepeaterBlock, 100);

#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const char *UNPOWERED_REPEATER = "minecraft:unpowered_repeater";
    const char *POWERED_REPEATER = "minecraft:powered_repeater";
}

bool RedstoneRepeaterBlock::matches(const std::string &identifier)
{
    return identifier == UNPOWERED_REPEATER || identifier == POWERED_REPEATER;
}

bool RedstoneRepeaterBlock::isPowered(const BlockState &state) const
{
    return state.mName == POWERED_REPEATER;
}

BlockState RedstoneRepeaterBlock::getPoweredState(const BlockState &state) const
{
    return BlockState(POWERED_REPEATER, state.mStates);
}

BlockState RedstoneRepeaterBlock::getUnpoweredState(const BlockState &state) const
{
    return BlockState(UNPOWERED_REPEATER, state.mStates);
}

bool RedstoneRepeaterBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player,
                                       const Vector3i &position, const BlockState &state) const
{
    RedstoneSystem::onRepeaterActivated(owner, owner.getLevelFor(player), position, state);
    return true;
}
