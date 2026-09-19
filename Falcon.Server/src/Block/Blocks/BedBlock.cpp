#include "Block/Blocks/BedBlock.h"

#include "Block/BlockData.h"
#include "Block/BlockIdentifier.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

bool BedBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bed" || BlockIdentifier::endsWith(identifier, "_bed");
}

void BedBlock::placeHeadPiece(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state,
                              int playerFacing) {
    if (!state.mStates.contains("head_piece_bit") || !RedstoneFace::isHorizontal(playerFacing))
        return;

    Level &level = owner.getLevel();
    const Vector3i head = RedstoneFace::relative(position, playerFacing);

    const BlockState existing = level.getBlockState(head.x, head.y, head.z);
    if (existing.mName != "minecraft:air") {
        const BlockData *data = BlockDataTable::find(existing.mName.c_str());
        if (data == nullptr || data->mSolid)
            return;
    }

    Tag states = state.mStates;
    states.putByte("head_piece_bit", 1);

    const BlockState headState(state.mName, states);
    level.setBlockState(head.x, head.y, head.z, headState);
    BlockActionHandler::broadcastBlockUpdate(owner, head, headState);
}
