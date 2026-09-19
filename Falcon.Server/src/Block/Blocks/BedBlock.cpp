#include "Block/Blocks/BedBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/BlockIdentifier.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Explosion.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const char *HEAD_PIECE_BIT = "head_piece_bit";
    const char *OCCUPIED_BIT = "occupied_bit";
    const char *DIRECTION = "direction";
    const double BED_EXPLOSION_SIZE = 5.0;

    bool isHeadPiece(const BlockState &state) {
        return state.mStates.getByte(HEAD_PIECE_BIT, 0) != 0;
    }

    bool isInsideAccessArea(const Vector3f &position, const Vector3i &head, const Vector3i &footOffset) {
        float minX = (float) head.x - 2.0f;
        float maxX = (float) head.x + 3.0f;
        float minZ = (float) head.z - 2.0f;
        float maxZ = (float) head.z + 3.0f;

        if (footOffset.x < 0)
            minX += (float) footOffset.x;
        else
            maxX += (float) footOffset.x;

        if (footOffset.z < 0)
            minZ += (float) footOffset.z;
        else
            maxZ += (float) footOffset.z;

        return position.x > minX && position.x < maxX
               && position.y > (float) head.y - 5.5f && position.y < (float) head.y + 2.5f
               && position.z > minZ && position.z < maxZ;
    }
}

bool BedBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bed" || BlockIdentifier::endsWith(identifier, "_bed");
}

int BedBlock::headFace(const BlockState &state) {
    switch (state.mStates.getInt(DIRECTION, 0)) {
        case 1:
            return RedstoneFace::WEST;
        case 2:
            return RedstoneFace::NORTH;
        case 3:
            return RedstoneFace::EAST;
        default:
            return RedstoneFace::SOUTH;
    }
}

void BedBlock::placeHeadPiece(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state,
                              int playerFacing) {
    if (!state.mStates.contains(HEAD_PIECE_BIT) || !RedstoneFace::isHorizontal(playerFacing))
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
    states.putByte(HEAD_PIECE_BIT, 1);

    const BlockState headState(state.mName, states);
    level.setBlockState(head.x, head.y, head.z, headState);
    BlockActionHandler::broadcastBlockUpdate(owner, head, headState);
}

bool BedBlock::findHead(Level &level, const Vector3i &position, const BlockState &state, Vector3i &head) {
    if (isHeadPiece(state)) {
        head = position;
        return true;
    }

    head = RedstoneFace::relative(position, headFace(state));
    const BlockState headState = level.getBlockState(head.x, head.y, head.z);
    return headState.mName == state.mName && isHeadPiece(headState)
           && headState.mStates.getInt(DIRECTION, 0) == state.mStates.getInt(DIRECTION, 0);
}

bool BedBlock::isValidAt(Level &level, const Vector3i &head) {
    const BlockState headState = level.getBlockState(head.x, head.y, head.z);
    if (!matches(headState.mName) || !isHeadPiece(headState))
        return false;

    const Vector3i foot = RedstoneFace::relative(head, RedstoneFace::opposite(headFace(headState)));
    const BlockState footState = level.getBlockState(foot.x, foot.y, foot.z);
    return footState.mName == headState.mName && !isHeadPiece(footState)
           && footState.mStates.getInt(DIRECTION, 0) == headState.mStates.getInt(DIRECTION, 0);
}

void BedBlock::setOccupied(ServerNetworkHandler &owner, const Vector3i &head, bool occupied) {
    Level &level = owner.getLevel();
    const BlockState headState = level.getBlockState(head.x, head.y, head.z);
    if (!matches(headState.mName))
        return;

    const Vector3i foot = RedstoneFace::relative(head, RedstoneFace::opposite(headFace(headState)));
    const Vector3i parts[2] = {head, foot};

    for (const Vector3i &part: parts) {
        const BlockState partState = level.getBlockState(part.x, part.y, part.z);
        if (partState.mName != headState.mName || !partState.mStates.contains(OCCUPIED_BIT))
            continue;

        Tag states = partState.mStates;
        states.putByte(OCCUPIED_BIT, occupied ? 1 : 0);
        const BlockState updated(partState.mName, states);
        level.setBlockState(part.x, part.y, part.z, updated);
        BlockActionHandler::broadcastBlockUpdate(owner, part, updated);
    }
}

bool BedBlock::use(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                   const BlockState &state) {
    Level &level = owner.getLevelFor(player);
    const bool shouldExplode = player.getDimension() != DimensionType::Overworld;

    Vector3i head;
    const bool valid = findHead(level, position, state, head);
    if (!valid && !shouldExplode) {
        player.sendTranslation("§7%tile.bed.notValid", {});
        return true;
    }

    if (shouldExplode) {
        level.setBlockState(position.x, position.y, position.z, BlockState("minecraft:air"));
        BlockActionHandler::broadcastBlockUpdate(owner, position,
                                                 level.getBlockState(position.x, position.y, position.z));
        breakOtherHalf(owner, level, position, state);

        Explosion explosion(owner, level, Vector3f((float) position.x, (float) position.y, (float) position.z),
                            BED_EXPLOSION_SIZE, nullptr, false);
        explosion.setIncendiary(true);
        explosion.explode();
        return true;
    }

    const BlockState overlay = level.getBlockStateAtLayer(position.x, position.y, position.z, 1);
    if (LiquidView(overlay).isWater())
        return true;

    const Vector3i footOffset = RedstoneFace::offset(RedstoneFace::opposite(headFace(state)));
    if (!isInsideAccessArea(player.getPosition(), head, footOffset)) {
        player.sendTranslation("§7%tile.bed.tooFar", {});
        return true;
    }

    player.setSpawnPoint(head);
    player.sendTranslation("§7%tile.bed.respawnSet", {});

    if (!level.isNight() && !level.isThundering()) {
        player.sendTranslation("§7%tile.bed.noSleep", {});
        return true;
    }

    if (!owner.sleepOn(player, head))
        player.sendTranslation("§7%tile.bed.occupied", {});

    return true;
}

void BedBlock::breakOtherHalf(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) {
    const int face = headFace(state);
    const Vector3i other = isHeadPiece(state) ? RedstoneFace::relative(position, RedstoneFace::opposite(face))
                                              : RedstoneFace::relative(position, face);

    const Vector3i head = isHeadPiece(state) ? position : other;
    owner.wakeSleepersAt(head);

    const BlockState otherState = level.getBlockState(other.x, other.y, other.z);
    if (otherState.mName != state.mName || isHeadPiece(otherState) == isHeadPiece(state)
        || otherState.mStates.getInt(DIRECTION, 0) != state.mStates.getInt(DIRECTION, 0))
        return;

    const BlockState air("minecraft:air");
    level.setBlockState(other.x, other.y, other.z, air);
    BlockActionHandler::broadcastBlockUpdate(owner, other, level.getBlockState(other.x, other.y, other.z));
}
