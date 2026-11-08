#include "World/Level/Block/Actor/EndGatewayBlockActor.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/BlockEventPacket.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Dimension/EndDragonFight.h"
#include "World/Level/Dimension/EndGatewayExit.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <cstdlib>
#include <vector>

namespace {
    const char *const TAG_AGE = "Age";
    const char *const TAG_EXIT_PORTAL = "ExitPortal";
    const char *const TAG_BAD_POSITION_CHECKED = "EndGatewayBadPosChecked";

    const char *const END_GATEWAY = "minecraft:end_gateway";

    // how far from the gateway an entity can stand and still touch its block
    const float ENTITY_SEARCH_RADIUS = 2.0f;

    int32_t listInt(const std::vector<Tag> &values, size_t index) {
        if (index >= values.size() || values[index].getType() != Tag::Type::Int)
            return 0;

        return values[index].asInt();
    }

    int64_t manhattan(const Vector3i &position) {
        return std::llabs((int64_t) position.x) + std::llabs((int64_t) position.y) + std::llabs((int64_t) position.z);
    }

    AxisAlignedBB boxAt(const Vector3f &position, float width, float height) {
        const float half = width * 0.5f;
        return AxisAlignedBB(position.x - half, position.y, position.z - half, position.x + half, position.y + height,
                             position.z + half);
    }
}

void EndGatewayBlockActor::_writeFields(Tag &data) const {
    data.putInt(TAG_AGE, mAge);

    Tag exit = Tag::ofList(Tag::Type::Int);
    exit.addToList(Tag::ofInt(mExit.x));
    exit.addToList(Tag::ofInt(mExit.y));
    exit.addToList(Tag::ofInt(mExit.z));
    data.put(TAG_EXIT_PORTAL, exit);

    // every save marks the exit as checked, so an old exit is only found again once
    data.putBool(TAG_BAD_POSITION_CHECKED, true);
}

Tag EndGatewayBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    _writeFields(data);
    return data;
}

Tag EndGatewayBlockActor::getSpawnCompound() const {
    // the game sends a gateway with its saved data
    Tag data = BlockActor::getSpawnCompound();
    _writeFields(data);
    return data;
}

void EndGatewayBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    mAge = data.getInt(TAG_AGE, 0);
    mExit = Vector3i(0, 0, 0);
    mSearchExit = false;
    mCheckExit = false;
    mBadPosition = false;

    const Tag *exit = data.get(TAG_EXIT_PORTAL);
    if (exit != nullptr && exit->isList()) {
        const std::vector<Tag> &values = exit->getList();
        mExit = Vector3i(listInt(values, 0), listInt(values, 1), listInt(values, 2));
    }

    if (!hasExit()) {
        mSearchExit = true;
        return;
    }

    mBadPosition = data.getByte(TAG_BAD_POSITION_CHECKED, 0) == 0 && isBadPosition(mExit, mPosition);
    mCheckExit = true;
}

bool EndGatewayBlockActor::hasExit() const {
    return !EndGatewayExit::isZero(mExit);
}

void EndGatewayBlockActor::setExit(const Vector3i &exit) {
    mExit = exit;
    mSearchExit = false;
    mCheckExit = false;
    mBadPosition = false;
}

bool EndGatewayBlockActor::isBadPosition(const Vector3i &exit, const Vector3i &gateway) {
    return manhattan(exit) <= BAD_EXIT_DISTANCE && manhattan(gateway) <= BAD_GATEWAY_DISTANCE;
}

Vector3f EndGatewayBlockActor::getTeleportTarget(const Vector3i &exit) {
    return Vector3f((float) exit.x + 0.5f, (float) (exit.y + 1), (float) exit.z + 0.5f);
}

bool EndGatewayBlockActor::startTick() {
    mPreviousAge = mAge;
    mWasCooling = mCooldown > 0;
    mAge++;
    if (!mWasCooling)
        return true;

    mCooldown--;
    return false;
}

bool EndGatewayBlockActor::needsClientUpdate() const {
    return (mPreviousAge < SPAWN_BEAM_TICKS) != (mAge < SPAWN_BEAM_TICKS) || mWasCooling != (mCooldown > 0);
}

bool EndGatewayBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return true;

    Level &level = *mLevel;
    const BlockState *state = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr)
        return true;

    if (state->mName != END_GATEWAY)
        return false;

    const bool open = startTick();

    // only a ticking gateway loads its other end, as in the game: a gateway loaded by the far end of another
    // one would otherwise load that end back
    if (level.isColumnActive(mPosition.x >> 4, mPosition.z >> 4))
        _requestExitWork(owner, level);

    if (open && level.getDimensionType() == DimensionType::TheEnd)
        _teleportEntities(owner, level);

    if (needsClientUpdate())
        BlockActionHandler::broadcastBlockActorData(owner, level, *this);

    return true;
}

void EndGatewayBlockActor::_requestExitWork(ServerNetworkHandler &owner, Level &level) {
    const bool inEnd = level.getDimensionType() == DimensionType::TheEnd;

    if (mSearchExit) {
        mSearchExit = false;
        if (inEnd)
            owner.getWorldOf(level).getDragonFight().requestGatewayExit(level, mPosition, false);
        return;
    }

    if (!mCheckExit)
        return;

    mCheckExit = false;
    if (!inEnd)
        return;

    if (mBadPosition) {
        mBadPosition = false;
        mExit = EndDragonFight::findRoughExit(level, mPosition);
    }

    owner.getWorldOf(level).getDragonFight().requestGatewayCheck(mPosition, mExit);
}

bool EndGatewayBlockActor::_canTeleport(const Actor &actor) {
    // ender pearls fly through, and the game never sends a rider or a ridden entity (sub_14714E830)
    if (dynamic_cast<const EnderPearlActor *>(&actor) != nullptr)
        return false;

    return !actor.isRiding() && !actor.hasPassengers();
}

void EndGatewayBlockActor::_teleportEntities(ServerNetworkHandler &owner, Level &level) {
    // the game would send entities to (0.5, 1, 0.5) before the exit is found, under the main island
    if (!hasExit())
        return;

    const AxisAlignedBB block((float) mPosition.x, (float) mPosition.y, (float) mPosition.z,
                              (float) mPosition.x + 1.0f, (float) mPosition.y + 1.0f, (float) mPosition.z + 1.0f);

    std::vector<Actor *> inside;
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.isAwaitingDimensionAck() || !player.isIn(level))
            continue;

        if (boxAt(player.getPosition(), MovementPhysics::PLAYER_WIDTH, MovementPhysics::PLAYER_HEIGHT)
                .intersectsWith(block))
            inside.push_back(&player);
    }

    const Vector3f center((float) mPosition.x + 0.5f, (float) mPosition.y + 0.5f, (float) mPosition.z + 0.5f);
    owner.forEachActorNear(center, ENTITY_SEARCH_RADIUS, [&](ServerActor &actor) {
        if (!actor.isIn(level) || !actor.isAlive())
            return;

        const ActorSize size = actor.getSize();
        if (boxAt(actor.getPosition(), size.mWidth, size.mHeight).intersectsWith(block))
            inside.push_back(&actor);
    });

    for (Actor *actor: inside) {
        if (mCooldown > 0)
            return;

        if (_canTeleport(*actor))
            _teleport(owner, level, *actor);
    }
}

void EndGatewayBlockActor::_teleport(ServerNetworkHandler &owner, Level &level, Actor &actor) {
    const Vector3f target = getTeleportTarget(mExit);

    if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&actor)) {
        player->teleport(owner, target);
    } else if (ServerActor *entity = dynamic_cast<ServerActor *>(&actor)) {
        entity->teleport(target);
        owner.broadcastActorMove(*entity, true);
    } else {
        return;
    }

    triggerCooldown();

    // the client starts its own cooldown beam from this event
    BlockEventPacket event;
    event.mBlockPosition = mPosition;
    event.mEventType = COOLDOWN_EVENT;
    event.mEventData = 0;
    BlockActionHandler::broadcastToViewers(owner, level,
                                           Vector3f((float) mPosition.x + 0.5f, (float) mPosition.y + 0.5f,
                                                    (float) mPosition.z + 0.5f),
                                           event);
}
