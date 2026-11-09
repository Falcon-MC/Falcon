#include "World/Level/Block/DoorOrientationBlock.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/DoorBlock.h"
#include "World/Level/Block/DoubleBlock.h"
#include "World/Level/Block/OpenableBlock.h"
#include "World/Level/Block/Components/BlockPlacementComponent.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

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

namespace {
    // south, west, north, east
    int32_t doorDirection(const BlockState &state) {
        const std::string cardinal = state.mStates.getString("minecraft:cardinal_direction", "");
        if (cardinal == "south") return 0;
        if (cardinal == "west") return 1;
        if (cardinal == "north") return 2;
        if (cardinal == "east") return 3;
        return state.mStates.getInt("direction", 0) & 3;
    }
}

bool DoorOrientationBlock::getCollisionShapesAt(
        const BlockState &state, const Vector3i &position, const Actor *actor,
        const std::function<const BlockState *(int32_t, int32_t, int32_t)> &neighbours,
        std::vector<AxisAlignedBB> &shapes) const {
    (void) actor;
    // the client resolves the plane from both halves: direction and open bit below, hinge above; a half alone
    // stands as a closed south door
    const bool upper = state.mStates.getByte("upper_block_bit") != 0;
    const BlockState *other = neighbours(position.x, position.y + (upper ? -1 : 1), position.z);
    int32_t direction = 0;
    bool open = false;
    bool hingeRight = false;
    if (other != nullptr && other->mName == state.mName) {
        const BlockState &lower = upper ? *other : state;
        const BlockState &top = upper ? state : *other;
        direction = doorDirection(lower);
        open = lower.mStates.getByte("open_bit") != 0;
        hingeRight = top.mStates.getByte("door_hinge_bit") != 0;
    }
    const int32_t plane = open ? (direction + (hingeRight ? 2 : 0)) & 3 : (direction + 3) & 3;
    const float thickness = 0.1825f;
    AxisAlignedBB box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    if (plane == 0)
        box.mMaxZ = thickness;
    else if (plane == 1)
        box.mMinX = 1.0f - thickness;
    else if (plane == 2)
        box.mMinZ = 1.0f - thickness;
    else
        box.mMaxX = thickness;
    shapes.push_back(box);
    return true;
}
