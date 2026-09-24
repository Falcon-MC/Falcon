#include "Block/Blocks/RedstoneComparatorBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(RedstoneComparatorBlock, 110);

#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const char *UNPOWERED_COMPARATOR = "minecraft:unpowered_comparator";
    const char *POWERED_COMPARATOR = "minecraft:powered_comparator";
}

bool RedstoneComparatorBlock::matches(const std::string &identifier)
{
    return identifier == UNPOWERED_COMPARATOR || identifier == POWERED_COMPARATOR;
}

bool RedstoneComparatorBlock::isPowered(const BlockState &state) const
{
    if (state.mName == POWERED_COMPARATOR)
        return true;

    return state.mStates.getBool("output_lit_bit", false);
}

BlockState RedstoneComparatorBlock::getPoweredState(const BlockState &state) const
{
    return BlockState(POWERED_COMPARATOR, state.mStates);
}

BlockState RedstoneComparatorBlock::getUnpoweredState(const BlockState &state) const
{
    return BlockState(UNPOWERED_COMPARATOR, state.mStates);
}

bool RedstoneComparatorBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player,
                                         const Vector3i &position, const BlockState &state) const
{
    RedstoneSystem::onComparatorActivated(owner, owner.getLevelFor(player), position, state);
    return true;
}
