#include "Block/Blocks/BedBlock.h"

#include "Actor/ActorCategory.h"
#include "Actor/ActorClassRegistry.h"
#include "Actor/ServerActor.h"
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

    bool isMonsterNearby(ServerNetworkHandler &owner, Level &level, const Vector3i &head,
                         const Vector3i &footOffset) {
        const AxisAlignedBB area = AxisAlignedBB((float) head.x - 8.0f, (float) head.y - 6.5f, (float) head.z - 8.0f,
                                                 (float) head.x + 9.0f, (float) head.y + 5.5f, (float) head.z + 9.0f)
                .addCoord((float) footOffset.x, 0.0f, (float) footOffset.z);

        for (const auto &entry: owner.getActors()) {
            const ServerActor *actor = entry.second.get();
            if (actor == nullptr || !actor->isAlive() || !ActorCategories::isPreventingSleep(*actor) ||
                actor->getDimension() != level.getDimensionType())
                continue;

            const Vector3f position = actor->getPosition();
            const ActorSize size = ActorClassRegistry::getSize(actor->getIdentifier());
            const float halfWidth = size.mWidth * 0.5f;
            const AxisAlignedBB box(position.x - halfWidth, position.y, position.z - halfWidth,
                                    position.x + halfWidth, position.y + size.mHeight, position.z + halfWidth);
            if (box.intersectsWith(area))
                return true;
        }

        return false;
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

void BedBlock::setOccupied(ServerNetworkHandler &owner, Level &level, const Vector3i &head, bool occupied) {
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
        BlockActionHandler::broadcastBlockUpdate(owner, level, part, updated);
    }
}

bool BedBlock::use(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                   const BlockState &state) {
    Level &level = owner.getLevelFor(player);
    const bool shouldExplode = player.getDimension() != DimensionType::Overworld;
    const bool willExplode = shouldExplode && level.getGameRules().getBool("respawnblocksexplode");

    Vector3i head;
    const bool valid = findHead(level, position, state, head);
    if (!valid) {
        if (!willExplode)
            player.sendTranslation("§7%tile.bed.notValid", {});

        if (!shouldExplode)
            return true;
    }

    if (shouldExplode) {
        if (!willExplode)
            return true;

        level.setBlockState(position.x, position.y, position.z, BlockState("minecraft:air"));
        BlockActionHandler::broadcastBlockUpdate(owner, level, position,
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

    if (player.getGameType() != (int32_t) GameType::Creative && isMonsterNearby(owner, level, head, footOffset)) {
        player.sendTranslation("§7%tile.bed.notSafe", {});
        return true;
    }

    if (!owner.sleepOn(player, head))
        player.sendTranslation("§7%tile.bed.occupied", {});

    return true;
}

std::vector<Vector3i> BedBlock::otherPiece(Level &level, const Vector3i &position, const BlockState &state) {
    const int face = headFace(state);
    const Vector3i other = isHeadPiece(state) ? RedstoneFace::relative(position, RedstoneFace::opposite(face))
                                              : RedstoneFace::relative(position, face);

    const BlockState otherState = level.getBlockState(other.x, other.y, other.z);
    if (otherState.mName != state.mName || isHeadPiece(otherState) == isHeadPiece(state)
        || otherState.mStates.getInt(DIRECTION, 0) != state.mStates.getInt(DIRECTION, 0))
        return {};

    return {other};
}

void BedBlock::breakOtherHalf(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) {
    const int face = headFace(state);
    const Vector3i head = isHeadPiece(state) ? position : RedstoneFace::relative(position, face);
    owner.wakeSleepersAt(head);

    for (const Vector3i &other: otherPiece(level, position, state)) {
        const BlockState air("minecraft:air");
        level.setBlockState(other.x, other.y, other.z, air);
        BlockActionHandler::broadcastBlockUpdate(owner, level, other, level.getBlockState(other.x, other.y, other.z));
    }
}
