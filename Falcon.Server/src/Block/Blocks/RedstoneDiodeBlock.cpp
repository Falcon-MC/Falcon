#include "Block/Blocks/RedstoneDiodeBlock.h"

#include "Block/BlockSupport.h"
#include "Block/Components/PlacementOrientation.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"

bool RedstoneDiodeBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const
{
    (void) blockFace;

    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    return BlockSupport::isAttachable(below, PlacementOrientation::FACE_UP) || below.mName == "minecraft:cauldron";
}

bool RedstoneRepeaterBlock::matches(const std::string &identifier)
{
    return identifier == "minecraft:unpowered_repeater" || identifier == "minecraft:powered_repeater";
}

bool RedstoneRepeaterBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player,
                                       const Vector3i &position, const BlockState &state) const
{
    (void) player;

    RedstoneSystem::onRepeaterActivated(owner, position, state);
    return true;
}

bool RedstoneComparatorBlock::matches(const std::string &identifier)
{
    return identifier == "minecraft:unpowered_comparator" || identifier == "minecraft:powered_comparator";
}

bool RedstoneComparatorBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player,
                                         const Vector3i &position, const BlockState &state) const
{
    (void) player;

    RedstoneSystem::onComparatorActivated(owner, position, state);
    return true;
}
