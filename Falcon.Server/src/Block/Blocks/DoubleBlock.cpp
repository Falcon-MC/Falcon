#include "Block/Blocks/DoubleBlock.h"

#include "Level/Level.h"

bool DoubleBlock::isUpperHalf(const BlockState &state) {
    return state.mStates.getByte("upper_block_bit") != 0;
}

std::vector<Vector3i> DoubleBlock::otherHalf(Level &level, const Vector3i &position, const BlockState &state) {
    if (!state.mStates.contains("upper_block_bit"))
        return {};

    const int32_t otherY = isUpperHalf(state) ? position.y - 1 : position.y + 1;
    if (otherY < level.getMinY() || otherY > level.getMaxY())
        return {};

    const Vector3i other(position.x, otherY, position.z);
    const BlockState otherState = level.getBlockState(other.x, other.y, other.z);

    if (otherState.mName != state.mName || isUpperHalf(otherState) == isUpperHalf(state))
        return {};

    return {other};
}

bool DoubleBlock::isComplete(Level &level, const Vector3i &position, const BlockState &state) {
    if (!state.mStates.contains("upper_block_bit"))
        return true;

    const int32_t otherY = isUpperHalf(state) ? position.y - 1 : position.y + 1;
    if (otherY < level.getMinY() || otherY > level.getMaxY())
        return false;

    const BlockState otherState = level.getBlockState(position.x, otherY, position.z);
    return otherState.mName == state.mName && isUpperHalf(otherState) != isUpperHalf(state);
}
