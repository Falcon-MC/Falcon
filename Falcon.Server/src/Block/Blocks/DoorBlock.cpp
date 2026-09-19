#include "Block/Blocks/DoorBlock.h"

#include "Block/BlockData.h"
#include "Block/BlockIdentifier.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    bool isTransparentAt(Level &level, const Vector3i &position) {
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (state.mName == "minecraft:air")
            return true;

        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data == nullptr || data->mTransparent;
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

bool DoorBlock::canPlaceUpperHalf(Level &level, const Vector3i &position) {
    if (position.y + 1 > LevelChunk::MAX_Y)
        return false;

    const BlockState above = level.getBlockState(position.x, position.y + 1, position.z);
    if (above.mName == "minecraft:air")
        return true;

    const BlockData *data = BlockDataTable::find(above.mName.c_str());
    return data != nullptr && !data->mSolid;
}

void DoorBlock::placeUpperHalf(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state) {
    if (!state.mStates.contains("upper_block_bit"))
        return;

    Level &level = owner.getLevel();
    if (!canPlaceUpperHalf(level, position))
        return;

    Tag states = state.mStates;
    states.putByte("upper_block_bit", 1);

    const Vector3i upper(position.x, position.y + 1, position.z);
    const BlockState upperState(state.mName, states);
    level.setBlockState(upper.x, upper.y, upper.z, upperState);
    BlockActionHandler::broadcastBlockUpdate(owner, upper, upperState);
}
