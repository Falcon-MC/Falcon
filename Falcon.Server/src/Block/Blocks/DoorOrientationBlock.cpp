#include "Block/Blocks/DoorOrientationBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Block/BlockSupport.h"
#include "Block/Blocks/DoorBlock.h"
#include "Block/Blocks/DoubleBlock.h"
#include "Block/Blocks/OpenableBlock.h"
#include "Block/Components/BlockPlacementComponent.h"
#include "Block/Components/PlacementOrientation.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

FALCON_REGISTER_BLOCK(DoorOrientationBlock, 120);

bool DoorOrientationBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace != PlacementOrientation::FACE_UP)
        return false;

    const BlockState above = level.getBlockState(position.x, position.y + 1, position.z);
    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    return BlockSupport::isReplaceable(above) && BlockSupport::isSolidOrCauldron(below);
}

bool DoorOrientationBlock::matches(const std::string &identifier) {
    return DoorBlock::matches(identifier);
}

bool DoorOrientationBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    if (!DoubleBlock::isComplete(level, position, state))
        return false;

    if (DoubleBlock::isUpperHalf(state))
        return true;

    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    return BlockSupport::isSolidOrCauldron(below);
}

std::vector<BlockPlacementEntry> DoorOrientationBlock::getPlacementBlocks(Level &level, const Vector3i &position,
                                                                         const BlockState &state,
                                                                         int playerFacing) const {
    (void) level;
    (void) playerFacing;

    if (!state.mStates.contains("upper_block_bit"))
        return {};

    Tag states = state.mStates;
    states.putByte("upper_block_bit", 1);

    return {BlockPlacementEntry{Vector3i(position.x, position.y + 1, position.z),
                                BlockState(state.mName, states)}};
}

bool DoorOrientationBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                      const BlockState &state) const {
    return DoorBlock::toggle(owner, owner.getLevelFor(player), position, state);
}

void DoorOrientationBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                    const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    (void) blockFace;

    Level &level = owner.getLevelFor(player);
    if (OpenableBlock::isOpen(state) || !DoorBlock::isGettingPower(owner, level, position, state))
        return;

    DoorBlock::setOpen(owner, level, position, state, true);
}

std::vector<Vector3i> DoorOrientationBlock::getAffectedBlocks(Level &level, const Vector3i &position,
                                                              const BlockState &state) const {
    return DoubleBlock::otherHalf(level, position, state);
}

PistonMoveReaction DoorOrientationBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

BlockState DoorOrientationBlock::applyPlacementOrientation(const BlockState &state,
                                                           const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;

    if (states.contains("minecraft:cardinal_direction"))
        states.putString("minecraft:cardinal_direction",
                         cardinalName(RedstoneFace::rotateY(context.mPlayerFacing)));

    if (states.contains("door_hinge_bit")) {
        const bool rightHinged = DoorBlock::isRightHinged(context.mLevel, result.mName, context.mBlockPosition,
                                                          context.mPlayerFacing);
        states.putByte("door_hinge_bit", rightHinged ? 1 : 0);
    }

    return BlockState(result.mName, states);
}
