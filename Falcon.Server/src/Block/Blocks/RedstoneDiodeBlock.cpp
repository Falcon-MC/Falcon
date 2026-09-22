#include "Block/Blocks/RedstoneDiodeBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(RedstoneRepeaterBlock, 100);
FALCON_REGISTER_BLOCK(RedstoneComparatorBlock, 110);

#include "Block/BlockSupport.h"
#include "Block/Components/PlacementOrientation.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const char *UNPOWERED_REPEATER = "minecraft:unpowered_repeater";
    const char *POWERED_REPEATER = "minecraft:powered_repeater";
    const char *UNPOWERED_COMPARATOR = "minecraft:unpowered_comparator";
    const char *POWERED_COMPARATOR = "minecraft:powered_comparator";
}

bool RedstoneDiodeBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const
{
    (void) blockFace;

    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    return BlockSupport::isAttachable(below, PlacementOrientation::FACE_UP) || below.mName == "minecraft:cauldron";
}

bool RedstoneDiodeBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const
{
    (void) state;

    return canPlaceAt(level, position, PlacementOrientation::FACE_UP);
}

bool RedstoneDiodeBlock::isSignalSource() const
{
    return true;
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
