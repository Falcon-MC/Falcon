#include "Block/Blocks/OrientationBlocks.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/BlockIdentifier.h"
#include "Block/Blocks/BedBlock.h"
#include "Block/Blocks/DoorBlock.h"
#include "Block/BlockSupport.h"
#include "Block/Components/BlockPlacementComponent.h"
#include "Block/Components/PlacementOrientation.h"
#include "Block/Systems/PistonSystem.h"
#include "Level/Level.h"

namespace {
    using BlockIdentifier::endsWith;
    using BlockIdentifier::endsWithAny;
    using BlockIdentifier::equalsAny;
    using BlockIdentifier::startsWith;

    void setFacingDirection(Tag &states, int facing) {
        if (states.contains("facing_direction"))
            states.putInt("facing_direction", facing);

        if (states.contains("minecraft:facing_direction"))
            states.putString("minecraft:facing_direction", PlacementOrientation::faceName(facing));
    }

    bool isSolidNeighbour(Level *level, const Vector3i &position) {
        if (level == nullptr)
            return false;

        const BlockState state = level->getBlockState(position.x, position.y, position.z);
        if (state.mName == "minecraft:air")
            return false;

        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data != nullptr && data->mSolid;
    }
}

bool FacingMachineBlock::matches(const std::string &identifier) {
    return equalsAny(identifier, {"minecraft:piston", "minecraft:sticky_piston", "minecraft:observer"});
}

BlockState FacingMachineBlock::applyPlacementOrientation(const BlockState &state,
                                                         const BlockPlacementContext &context) const {
    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    setFacingDirection(states, context.mPistonFacing);
    return BlockState(result.mName, states);
}

bool PistonBlock::matches(const std::string &identifier) {
    return PistonSystem::isPiston(identifier);
}

void PistonBlock::onBroken(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state) const {
    PistonSystem::onBlockBroken(owner, position, state);
}

bool TorchOrientationBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:torch"
           || startsWith(identifier, "minecraft:colored_torch_")
           || endsWith(identifier, "_torch");
}

BlockState TorchOrientationBlock::applyPlacementOrientation(const BlockState &state,
                                                            const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    const int facing = context.mFace >= FACE_NORTH ? context.mFace : FACE_UP;
    setFacingDirection(states, facing);
    return BlockState(result.mName, states);
}

bool TorchOrientationBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    const int face = blockFace >= PlacementOrientation::FACE_NORTH ? blockFace : PlacementOrientation::FACE_UP;
    const Vector3i supportPosition = BlockSupport::supportOf(position, face);
    const BlockState support = level.getBlockState(supportPosition.x, supportPosition.y, supportPosition.z);
    return BlockSupport::isAttachable(support, face);
}

bool DoorOrientationBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace != PlacementOrientation::FACE_UP)
        return false;

    const BlockState above = level.getBlockState(position.x, position.y + 1, position.z);
    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    return BlockSupport::isReplaceable(above) && BlockSupport::isSolidOrCauldron(below);
}

bool WallAttachedBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:ladder"
           || equalsAny(identifier, {"minecraft:wall_sign", "minecraft:wall_banner"})
           || endsWithAny(identifier, {"_wall_sign", "_coral_fan", "_coral_wall_fan"})
           || identifier == "minecraft:coral_fan";
}

BlockState WallAttachedBlock::applyPlacementOrientation(const BlockState &state,
                                                        const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    const int facing = context.mFace >= FACE_NORTH ? context.mFace : context.mOppositeFacing;
    setFacingDirection(states, facing);
    return BlockState(result.mName, states);
}

bool BellOrientationBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bell";
}

BlockState BellOrientationBlock::applyPlacementOrientation(const BlockState &state,
                                                           const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    const int facing = context.mFace >= FACE_NORTH ? context.mFace : context.mOppositeFacing;
    setFacingDirection(states, facing);

    if (states.contains("attachment") && context.mFace != FACE_UP && context.mFace != FACE_DOWN
        && isSolidNeighbour(context.mLevel, relativePosition(context.mBlockPosition, context.mFace)))
        states.putString("attachment", "multiple");

    return BlockState(result.mName, states);
}

bool FaceAttachedBlock::matches(const std::string &identifier) {
    return endsWithAny(identifier, {"_amethyst_bud", "_cluster"})
           || equalsAny(identifier, {"minecraft:creeper_head", "minecraft:dragon_head",
                                     "minecraft:piglin_head", "minecraft:player_head",
                                     "minecraft:skeleton_skull", "minecraft:wither_skeleton_skull",
                                     "minecraft:zombie_head"});
}

BlockState FaceAttachedBlock::applyPlacementOrientation(const BlockState &state,
                                                        const BlockPlacementContext &context) const {
    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    setFacingDirection(states, context.mFace);
    return BlockState(result.mName, states);
}

bool CardinalPlayerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bed" || endsWith(identifier, "_bed")
           || identifier == "minecraft:fence_gate" || endsWith(identifier, "_fence_gate");
}

BlockState CardinalPlayerBlock::applyPlacementOrientation(const BlockState &state,
                                                          const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    if (states.contains("minecraft:cardinal_direction"))
        states.putString("minecraft:cardinal_direction", cardinalName(context.mPlayerFacing));
    return BlockState(result.mName, states);
}

bool BedOrientationBlock::matches(const std::string &identifier) {
    return BedBlock::matches(identifier);
}

void BedOrientationBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                   const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    (void) blockFace;

    BedBlock::placeHeadPiece(owner, position, state,
                             BlockPlacementComponent::getHorizontalFacing(player.getRotation().y));
}

bool DoorOrientationBlock::matches(const std::string &identifier) {
    return DoorBlock::matches(identifier);
}

void DoorOrientationBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                    const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) player;
    (void) usedItem;
    (void) blockFace;

    DoorBlock::placeUpperHalf(owner, position, state);
}

BlockState DoorOrientationBlock::applyPlacementOrientation(const BlockState &state,
                                                           const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;

    if (states.contains("minecraft:cardinal_direction"))
        states.putString("minecraft:cardinal_direction", cardinalName(context.mPlayerFacing));

    if (states.contains("door_hinge_bit")) {
        const bool rightHinged = DoorBlock::isRightHinged(context.mLevel, result.mName, context.mBlockPosition,
                                                          context.mPlayerFacing);
        states.putByte("door_hinge_bit", rightHinged ? 1 : 0);
    }

    return BlockState(result.mName, states);
}

bool TrapdoorOrientationBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:trapdoor" || endsWith(identifier, "_trapdoor");
}

BlockState TrapdoorOrientationBlock::applyPlacementOrientation(const BlockState &state,
                                                               const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    if (states.contains("direction"))
        states.putInt("direction", horizontalOrdinal(context.mOppositeFacing));
    return BlockState(result.mName, states);
}
