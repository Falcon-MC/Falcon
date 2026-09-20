#include "Block/Blocks/DoorBlock.h"

#include "Block/BlockData.h"
#include "Block/BlockIdentifier.h"
#include "Block/Blocks/OpenableBlock.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    bool isTransparentAt(Level &level, const Vector3i &position) {
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (state.mName == "minecraft:air")
            return true;

        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data == nullptr || data->mTransparent;
    }

    BlockState stateAt(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y, position.z);
    }
}

bool DoorBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_door");
}

bool DoorBlock::isRightHinged(Level *level, const std::string &identifier, const Vector3i &position,
                              int playerFacing) {
    if (level == nullptr)
        return false;

    const int leftFace = RedstoneFace::rotateYCounterClockwise(playerFacing);
    const int rightFace = RedstoneFace::rotateY(playerFacing);
    if (leftFace == RedstoneFace::NONE || rightFace == RedstoneFace::NONE)
        return false;

    const Vector3i left = RedstoneFace::relative(position, leftFace);
    const Vector3i right = RedstoneFace::relative(position, rightFace);

    if (level->getBlockState(left.x, left.y, left.z).mName == identifier)
        return true;

    return !isTransparentAt(*level, right) && isTransparentAt(*level, left);
}

Vector3i DoorBlock::lowerPosition(Level &level, const Vector3i &position, const BlockState &state) {
    if (!matches(state.mName))
        return position;

    const Tag *upper = state.mStates.get("upper_block_bit");
    const bool isUpper = upper != nullptr && upper->getType() == Tag::Type::Byte && upper->asByte() != 0;
    if (!isUpper)
        return position;

    const Vector3i below = RedstoneFace::relative(position, RedstoneFace::DOWN);
    return stateAt(level, below).mName == state.mName ? below : position;
}

bool DoorBlock::isGettingPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) {
    const Vector3i lower = lowerPosition(level, position, state);
    const Vector3i upper = RedstoneFace::relative(lower, RedstoneFace::UP);

    return RedstoneSystem::isGettingPower(owner, level, lower)
           || RedstoneSystem::isGettingPower(owner, level, upper);
}

bool DoorBlock::setOpen(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                        const BlockState &state, bool open) {
    const Vector3i lower = lowerPosition(level, position, state);
    const Vector3i upper = RedstoneFace::relative(lower, RedstoneFace::UP);

    const BlockState lowerState = stateAt(level, lower);
    const BlockState upperState = stateAt(level, upper);
    if (lowerState.mName != upperState.mName)
        return false;

    OpenableBlock::setOpen(owner, level, lower, lowerState, open);
    OpenableBlock::setOpen(owner, level, upper, upperState, open);

    return true;
}

bool DoorBlock::toggle(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                       const BlockState &state) {
    const Vector3i lower = lowerPosition(level, position, state);
    const BlockState lowerState = stateAt(level, lower);
    const bool open = !OpenableBlock::isOpen(lowerState);

    if (!setOpen(owner, level, position, state, open))
        return false;

    const Vector3i upper = RedstoneFace::relative(lower, RedstoneFace::UP);
    const bool manual = open || isGettingPower(owner, level, lower, lowerState);
    OpenableBlock::setManualOverride(level, lower, manual);
    OpenableBlock::setManualOverride(level, upper, manual);

    return true;
}

void DoorBlock::onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state) {
    const bool manualOverride = OpenableBlock::hasManualOverride(level, position);
    const bool gettingPower = isGettingPower(owner, level, position, state);
    const bool open = OpenableBlock::isOpen(state);

    if (open != gettingPower && !manualOverride) {
        OpenableBlock::setOpen(owner, level, position, state, gettingPower);

        const Vector3i lower = lowerPosition(level, position, state);
        const Vector3i upper = RedstoneFace::relative(lower, RedstoneFace::UP);
        const Vector3i other = lower == position ? upper : lower;
        const BlockState otherState = stateAt(level, other);
        if (otherState.mName == state.mName && OpenableBlock::isOpen(otherState) != gettingPower)
            OpenableBlock::setOpen(owner, level, other, otherState, gettingPower);

        return;
    }

    if (manualOverride && gettingPower == open)
        OpenableBlock::setManualOverride(level, position, false);
}
