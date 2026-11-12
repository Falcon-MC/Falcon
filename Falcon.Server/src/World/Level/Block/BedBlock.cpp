#include "World/Level/Block/BedBlock.h"

#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockQuery.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/LavaBlock.h"
#include "World/Level/Block/MagmaBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginManager.h"
#include "World/Level/Block/BlockNames.h"

namespace {
    const char *HEAD_PIECE_BIT = "head_piece_bit";
    const char *OCCUPIED_BIT = "occupied_bit";
    const char *DIRECTION = "direction";
    const char *CARDINAL_DIRECTION = "cardinal_direction";
    const double BED_EXPLOSION_SIZE = 5.0;

    struct StandUpOffset {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
    };

    // qword_14F1806E0
    const StandUpOffset STAND_UP_OFFSETS[12] = {{1, 0, 0},  {1, 0, 1},   {1, 0, 2},  {0, 0, 2},  {-1, 0, 2}, {-1, 0, 1},
                                                {-1, 0, 0}, {-1, 0, -1}, {0, 0, -1}, {1, 0, -1}, {0, 1, 0},  {0, 1, 1}};

    // qword_14F180770, taken when the sleeper lay down from the other side
    const StandUpOffset MIRRORED_STAND_UP_OFFSETS[12] = {{-1, 0, 0}, {-1, 0, 1},  {-1, 0, 2}, {0, 0, 2},
                                                         {1, 0, 2},  {1, 0, 1},   {1, 0, 0},  {1, 0, -1},
                                                         {0, 0, -1}, {-1, 0, -1}, {0, 1, 0},  {0, 1, 1}};

    // byte_14F0F3DA0, indexed by the direction state
    const int STAND_UP_ROTATIONS[4] = {2, 3, 0, 1};

    // the direction state the face was read from
    int directionIndex(int face) {
        switch (face) {
            case RedstoneFace::WEST:
                return 1;
            case RedstoneFace::NORTH:
                return 2;
            case RedstoneFace::EAST:
                return 3;
            default:
                return 0;
        }
    }

    // the magma flag (+306 & 0x40, set only for magma in sub_14CE9AD30), the lava material (type 6) and the wither rose
    bool isUnsafeToStandOn(const BlockState &state) {
        return VanillaBlocks::getAs<MagmaBlock>(state.mName) != nullptr || LavaBlock::matches(state.mName)
               || state.mName == BlockNames::WITHER_ROSE;
    }

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

        return position.x > minX && position.x < maxX && position.y > (float) head.y - 5.5f
               && position.y < (float) head.y + 2.5f && position.z > minZ && position.z < maxZ;
    }

    bool isMonsterNearby(ServerNetworkHandler &owner, Level &level, const Vector3i &head, const Vector3i &footOffset) {
        const AxisAlignedBB area = AxisAlignedBB((float) head.x - 8.0f, (float) head.y - 6.5f, (float) head.z - 8.0f,
                                                 (float) head.x + 9.0f, (float) head.y + 5.5f, (float) head.z + 9.0f)
                                           .addCoord((float) footOffset.x, 0.0f, (float) footOffset.z);

        for (const auto &entry: owner.getActors()) {
            const MobActor *actor = dynamic_cast<const MobActor *>(entry.second.get());
            if (actor == nullptr || !actor->isAlive() || !actor->preventsSleep() || !actor->isIn(level))
                continue;

            const Vector3f position = actor->getPosition();
            const ActorSize size = actor->getSize();
            const float halfWidth = size.mWidth * 0.5f;
            const AxisAlignedBB box(position.x - halfWidth, position.y, position.z - halfWidth, position.x + halfWidth,
                                    position.y + size.mHeight, position.z + halfWidth);
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
    if (state.mStates.contains(CARDINAL_DIRECTION))
        return PlacementOrientation::faceFromName(state.mStates.getString(CARDINAL_DIRECTION, "south"));

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
    return headState.mName == state.mName && isHeadPiece(headState) && headFace(headState) == headFace(state);
}

bool BedBlock::isValidAt(Level &level, const Vector3i &head) {
    const BlockState headState = level.getBlockState(head.x, head.y, head.z);
    if (!matches(headState.mName) || !isHeadPiece(headState))
        return false;

    const Vector3i foot = RedstoneFace::relative(head, RedstoneFace::opposite(headFace(headState)));
    const BlockState footState = level.getBlockState(foot.x, foot.y, foot.z);
    return footState.mName == headState.mName && !isHeadPiece(footState) && headFace(footState) == headFace(headState);
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

        level.setBlockState(position.x, position.y, position.z, BlockState(BlockNames::AIR));
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

    player.setSpawnPoint(head, player.getSpawnStandPositionHere(), player.getDimension(), level.getName());
    player.sendTranslation("§7%tile.bed.respawnSet", {});

    if (!level.isNight() && !level.isThundering()) {
        player.sendTranslation("§7%tile.bed.noSleep", {});
        return true;
    }

    if (!player.isCreative() && isMonsterNearby(owner, level, head, footOffset)) {
        player.sendTranslation("§7%tile.bed.notSafe", {});
        return true;
    }

    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_PLAYER_BED_ENTER)) {
        PluginEvent bedEvent;
        bedEvent.mType = FALCON_EVENT_PLAYER_BED_ENTER;
        bedEvent.mCancellable = true;
        bedEvent.mPlayer = &player;
        bedEvent.mBlockPosition = head;
        plugins->dispatch(bedEvent);
        if (bedEvent.mCancelled)
            return true;
    }

    // kept as the actor's position, at the eyes, only once the player lies down (sub_143155010)
    const Vector3f previousEnterPosition = player.getBedEnterPosition();
    const Vector3f feet = player.getPosition();
    player.setBedEnterPosition(Vector3f(feet.x, feet.y + MovementPhysics::PLAYER_EYE_HEIGHT, feet.z));
    if (!owner.sleepOn(player, head)) {
        player.setBedEnterPosition(previousEnterPosition);
        player.sendTranslation("§7%tile.bed.occupied", {});
    }

    return true;
}

bool BedBlock::findStandUpPosition(Level &level, const Vector3i &bed, const Vector3f &previous, Vector3i &standUp) {
    const BlockState bedState = level.getBlockState(bed.x, bed.y, bed.z);
    if (!matches(bedState.mName))
        return false;

    Vector3i head = bed;
    if (!findHead(level, bed, bedState, head))
        head = bed;

    const int direction = directionIndex(headFace(level.getBlockState(head.x, head.y, head.z)));
    const int rotation = STAND_UP_ROTATIONS[direction];
    const float centerX = (float) head.x + 0.5f;
    const float centerZ = (float) head.z + 0.5f;
    bool mirrored = false;
    switch (direction) {
        case 0:
            mirrored = previous.x > centerX;
            break;
        case 1:
            mirrored = previous.z > centerZ;
            break;
        case 2:
            mirrored = previous.x < centerX;
            break;
        default:
            mirrored = previous.z < centerZ;
            break;
    }
    const StandUpOffset *offsets = mirrored ? MIRRORED_STAND_UP_OFFSETS : STAND_UP_OFFSETS;

    bool hasFallback = false;
    Vector3i fallback;
    for (int i = 0; i < 12; ++i) {
        const int32_t a = offsets[i].mX;
        const int32_t b = offsets[i].mZ;
        int32_t dx = a;
        int32_t dz = b;
        switch (rotation) {
            case 1:
                dx = -b;
                dz = a;
                break;
            case 2:
                dx = -a;
                dz = -b;
                break;
            case 3:
                dx = b;
                dz = -a;
                break;
            default:
                break;
        }
        const Vector3i candidate(head.x + dx, head.y + offsets[i].mY, head.z + dz);
        const Vector3i below(candidate.x, candidate.y - 1, candidate.z);

        const BlockState belowState = level.getBlockState(below.x, below.y, below.z);
        const AxisAlignedBB floor = BlockShape::getRelativeShape(belowState);
        if (!(floor.mMinX < floor.mMaxX && floor.mMinY < floor.mMaxY && floor.mMinZ < floor.mMaxZ))
            continue;

        const BlockState candidateState = level.getBlockState(candidate.x, candidate.y, candidate.z);
        const bool candidateIsBed = matches(candidateState.mName);
        // BlockSource vfunc 432 (sub_1432D1C00)
        if (!candidateIsBed
            && (BlockSupport::isSolid(candidateState) || candidateState.mName == BlockNames::END_PORTAL
                || candidateState.mName == BlockNames::END_PORTAL_FRAME))
            continue;

        if (!BlockQuery::isAirAt(level, Vector3i(candidate.x, candidate.y + 1, candidate.z)))
            continue;
        if (candidateIsBed && !BlockQuery::isAirAt(level, Vector3i(candidate.x, candidate.y + 2, candidate.z)))
            continue;

        if (!isUnsafeToStandOn(candidateState) && !isUnsafeToStandOn(belowState)) {
            standUp = candidate;
            return true;
        }
        if (!hasFallback) {
            hasFallback = true;
            fallback = candidate;
        }
    }

    if (hasFallback)
        standUp = fallback;
    return hasFallback;
}

std::vector<Vector3i> BedBlock::otherPiece(Level &level, const Vector3i &position, const BlockState &state) {
    const int face = headFace(state);
    const Vector3i other = isHeadPiece(state) ? RedstoneFace::relative(position, RedstoneFace::opposite(face))
                                              : RedstoneFace::relative(position, face);

    const BlockState otherState = level.getBlockState(other.x, other.y, other.z);
    if (otherState.mName != state.mName || isHeadPiece(otherState) == isHeadPiece(state)
        || headFace(otherState) != headFace(state))
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
