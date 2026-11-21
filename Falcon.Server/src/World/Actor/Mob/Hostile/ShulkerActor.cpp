#include "World/Actor/Mob/Hostile/ShulkerActor.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/DamageCause.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <cmath>
#include <random>
#include <string>

FALCON_REGISTER_ACTOR(ShulkerActor, ShulkerActor::IDENTIFIER);

namespace {
    // the step to the block each face is stuck to, in Facing order
    const int32_t FACE_STEPS[ShulkerActor::FACE_COUNT][3] = {{0, -1, 0}, {0, 1, 0},  {0, 0, -1},
                                                            {0, 0, 1},  {-1, 0, 0}, {1, 0, 0}};
    const char *const PEEK_COMPONENT = "minecraft:peek";
    const char *const BULLET_IDENTIFIER = "minecraft:shulker_bullet";
    const char *const TAG_ATTACH_POSITION = "AttachPos";
    const char *const TAG_ATTACH_FACE = "AttachFace";
    const char *const TAG_PEEK = "Peek";
    // one tick in this many, a shulker with no target opens up to look around
    const int32_t IDLE_PEEK_CHANCE = 40;
    // a look around lasts this many ticks, plus up to twice as many more in steps of it
    const int32_t IDLE_PEEK_TICKS = 20;
    const int32_t IDLE_PEEK_STEPS = 3;
    // below half health, one hit in this many teleports it
    const int32_t HURT_TELEPORT_CHANCE = 4;
    // a bullet hit counts the shulkers within this many of its sizes around it
    const float CROWD_REACH = 4.0f;
    const float CROWD_DIVISOR = 5.0f;
    const float SPAWN_YAW = 180.0f;

    std::mt19937 &shulkerRandom() {
        static thread_local std::mt19937 random(std::random_device{}());
        return random;
    }

    int32_t randomBelow(int32_t bound) {
        return std::uniform_int_distribution<int32_t>(0, bound - 1)(shulkerRandom());
    }

    Vector3i step(const Vector3i &position, int8_t face, int32_t sign) {
        const int32_t *offset = FACE_STEPS[face];
        return Vector3i(position.x + offset[0] * sign, position.y + offset[1] * sign, position.z + offset[2] * sign);
    }

    bool isSolid(Level &level, const Vector3i &position) {
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (state.mName == "minecraft:air")
            return false;
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data != nullptr && data->mSolid;
    }

    Vector3i blockOf(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }

    // the game takes its block from half a block up, so a shulker standing on a slab keeps the slab's cell
    Vector3i attachBlockOf(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y + 0.5f),
                        (int32_t) std::floor(position.z));
    }

    Vector3f centreOf(const Vector3i &block) {
        return Vector3f((float) block.x + 0.5f, (float) block.y, (float) block.z + 0.5f);
    }

    bool sameBlock(const Vector3i &a, const Vector3i &b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
}

void ShulkerActor::finalizeSpawn() {
    HostileActor::finalizeSpawn();
    const Vector3f rotation = getRotation();
    setRotation(Vector3f(rotation.x, SPAWN_YAW, SPAWN_YAW));
}

void ShulkerActor::tick(ServerNetworkHandler &owner) {
    HostileActor::tick(owner);
    if (isAlive())
        _tickPeek(owner);
}

// with no target the shell opens now and then for a while; a target opens it wide
void ShulkerActor::_tickPeek(ServerNetworkHandler &owner) {
    const bool hasTarget = getTarget(owner) != nullptr;
    const json::Value *peek = getComponent(PEEK_COMPONENT);
    Level &level = owner.getLevelFor(*this);

    if (!hasTarget && mPeekDuration <= 0 && randomBelow(IDLE_PEEK_CHANCE) == 0) {
        mPeekDuration = IDLE_PEEK_TICKS * randomBelow(IDLE_PEEK_STEPS) + IDLE_PEEK_TICKS;
        _setPeek(owner, PEEK_IDLE);
        if (peek != nullptr)
            EntityEvents::fireTrigger(owner, *this, peek->get("on_open"));
        owner.playLevelSound(level, "shulker.open", getPosition(), getIdentifier());
        level.postGameEvent(GameEvents::CONTAINER_OPEN, getPosition(), this);
    }

    if (mPeekDuration > 0)
        --mPeekDuration;

    if (!hasTarget && mPeekDuration == 0 && mPeek != PEEK_CLOSED) {
        _setPeek(owner, PEEK_CLOSED);
        if (peek != nullptr)
            EntityEvents::fireTrigger(owner, *this, peek->get("on_close"));
        owner.playLevelSound(level, "shulker.close", getPosition(), getIdentifier());
        level.postGameEvent(GameEvents::CONTAINER_CLOSE, getPosition(), this);
    }

    if (hasTarget && !mHadTarget) {
        _setPeek(owner, PEEK_OPEN);
        if (peek != nullptr)
            EntityEvents::fireTrigger(owner, *this, peek->get("on_target_open"));
        owner.playLevelSound(level, "shulker.open", getPosition(), getIdentifier());
    }
    mHadTarget = hasTarget;
}

// the game's order: its block taken on the first tick, then kept while the face holds and the shell has room,
// else another face of the same block, else a teleport; then it sits on the block's floor, centred
void ShulkerActor::tickMovement(ServerNetworkHandler &owner) {
    if (isRiding()) {
        _setAttachFace(owner, FACE_DOWN);
        HostileActor::tickMovement(owner);
        return;
    }

    if (!mAttached)
        _setAttachPosition(owner, attachBlockOf(getPosition()));
    // the game moves on from a piston in its hurt, which suffocation inside the arm brings every tick; no such
    // hurt comes here, so the same check runs on the tick
    _pushedByPiston(owner);

    Level &level = owner.getLevelFor(*this);
    const bool held = _holds(level, step(mAttachPosition, mAttachFace, 1), mAttachFace);
    const bool roomy = _isOpenCell(owner, step(mAttachPosition, mAttachFace, -1));
    const bool crowded = _shulkerAt(owner, mAttachPosition);
    const bool stranded = !(held && roomy) && !_findAttachment(owner, mAttachPosition);
    if (stranded || crowded)
        teleport(owner);

    const Vector3f centre = centreOf(mAttachPosition);
    const Vector3f before = getPosition();
    setMotion(Vector3f(0.0f, 0.0f, 0.0f));
    if (before.x != centre.x || before.y != centre.y || before.z != centre.z) {
        setPosition(centre);
        owner.broadcastActorMove(*this, true);
    }
}

bool ShulkerActor::_holds(Level &level, const Vector3i &support, int8_t face) const {
    const BlockState state = level.getBlockState(support.x, support.y, support.z);
    // a block a piston is moving holds it on any face
    if (state.mName == "minecraft:moving_block")
        return true;
    // a piston is no floor: it is pushed off one standing on it
    return isSolid(level, support) && (face != FACE_DOWN || state.mName != "minecraft:piston");
}

bool ShulkerActor::_isOpenCell(ServerNetworkHandler &owner, const Vector3i &cell) const {
    return !isSolid(owner.getLevelFor(*this), cell) && !_shulkerAt(owner, cell);
}

bool ShulkerActor::_shulkerAt(ServerNetworkHandler &owner, const Vector3i &cell) const {
    for (const auto &entry: owner.getActors()) {
        const auto *other = dynamic_cast<const ShulkerActor *>(entry.second.get());
        if (other == nullptr || other == this || !other->isAlive() || !other->sharesLevelWith(*this))
            continue;
        if (sameBlock(blockOf(other->getPosition()), cell))
            return true;
    }
    return false;
}

bool ShulkerActor::_findAttachment(ServerNetworkHandler &owner, const Vector3i &position) {
    Level &level = owner.getLevelFor(*this);
    for (int8_t face = 0; face < FACE_COUNT; ++face) {
        if (!_holds(level, step(position, face, 1), face) || !_isOpenCell(owner, step(position, face, -1)))
            continue;
        _setAttachFace(owner, face);
        return true;
    }
    return false;
}

bool ShulkerActor::teleport(ServerNetworkHandler &owner) {
    if (!isAlive())
        return true;
    const Vector3f position = getPosition();
    const Vector3i from = attachBlockOf(position);
    for (int32_t attempt = 0; attempt < TELEPORT_TRIES; ++attempt) {
        const int32_t x = randomBelow(TELEPORT_RANGE * 2 + 1);
        const int32_t y = randomBelow(TELEPORT_RANGE * 2 + 1);
        const int32_t z = randomBelow(TELEPORT_RANGE * 2 + 1);
        const Vector3i target(from.x - TELEPORT_RANGE + x, from.y - TELEPORT_RANGE + y, from.z - TELEPORT_RANGE + z);
        if (teleportTo(owner, target))
            return true;
    }
    return false;
}

// an air block above the world's floor, free of shulkers, with a face to stick to; the shell shuts and it
// forgets its target
bool ShulkerActor::teleportTo(ServerNetworkHandler &owner, const Vector3i &target) {
    Level &level = owner.getLevelFor(*this);
    if (level.getBlockState(target.x, target.y, target.z).mName != "minecraft:air")
        return false;
    if (target.y <= level.getMinY())
        return false;
    if (_shulkerAt(owner, target))
        return false;
    if (!_findAttachment(owner, target))
        return false;

    owner.playLevelSound(level, "teleport", getPosition(), getIdentifier());
    level.postGameEvent(GameEvents::TELEPORT, getPosition(), this);
    _setAttachPosition(owner, target);
    _setPeek(owner, PEEK_CLOSED);
    clearTarget();
    return true;
}

// the game's hurt: fire and lava send it away unharmed, and a shut shell turns projectiles aside
bool ShulkerActor::senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) {
    mHitByBullet = false;
    if (DamageCause::matches("fire", source) || DamageCause::matches("lava", source)) {
        teleport(owner);
        return false;
    }
    if (source.mProjectile && mPeek == PEEK_CLOSED)
        return false;
    if (_pushedByPiston(owner))
        return false;
    mHitByBullet = source.mDamager != nullptr && std::string(source.mDamager->getIdentifier()) == BULLET_IDENTIFIER;
    return HostileActor::senseDamage(owner, amount, source);
}

// the game's hurt, whatever the cause: a piston or its arm in the shulker's block moves the block one step against
// the piston's stored facing_direction, on every axis, and the shulker takes no damage
bool ShulkerActor::_pushedByPiston(ServerNetworkHandler &owner) {
    if (!mAttached)
        return false;
    const BlockState state =
            owner.getLevelFor(*this).getBlockState(mAttachPosition.x, mAttachPosition.y, mAttachPosition.z);
    if (state.mName != "minecraft:piston" && state.mName != "minecraft:piston_arm_collision")
        return false;
    const Tag *facing = state.mStates.get("facing_direction");
    const int32_t stored = facing == nullptr ? 0 : facing->asInt();
    if (stored < 0 || stored >= FACE_COUNT)
        return false;
    _setAttachPosition(owner, step(mAttachPosition, (int8_t) stored, -1));
    return true;
}

// hurt below half health it may flee; an open shulker its own kind's bullet strikes moves and may leave a copy
void ShulkerActor::onDamaged(ServerNetworkHandler &owner, Actor *attacker) {
    HostileActor::onDamaged(owner, attacker);
    const bool hitByBullet = mHitByBullet;
    mHitByBullet = false;
    if (!isAlive())
        return;

    if ((float) (int32_t) getHealth() < (float) (int32_t) getMaxHealth() * 0.5f
        && randomBelow(HURT_TELEPORT_CHANCE) == 0)
        teleport(owner);

    if (!hitByBullet || mPeek == PEEK_CLOSED)
        return;
    const Vector3f at = getPosition();
    if (teleport(owner))
        _duplicate(owner, at);
}

// the more shulkers around where it was, the less likely a new one takes its old place
void ShulkerActor::_duplicate(ServerNetworkHandler &owner, const Vector3f &at) {
    const ActorSize size = getSize();
    const float reachX = size.mWidth * CROWD_REACH;
    const float reachY = size.mHeight * CROWD_REACH;
    const float middleY = at.y + size.mHeight * 0.5f;
    int32_t crowd = 0;
    for (const auto &entry: owner.getActors()) {
        const auto *other = dynamic_cast<const ShulkerActor *>(entry.second.get());
        if (other == nullptr || other == this || !other->isAlive() || !other->sharesLevelWith(*this))
            continue;
        const Vector3f position = other->getPosition();
        const ActorSize otherSize = other->getSize();
        const float halfWidth = otherSize.mWidth * 0.5f;
        if (position.x + halfWidth >= at.x - reachX && position.x - halfWidth <= at.x + reachX
            && position.z + halfWidth >= at.z - reachX && position.z - halfWidth <= at.z + reachX
            && position.y + otherSize.mHeight >= middleY - reachY && position.y <= middleY + reachY)
            ++crowd;
    }

    const float chance = std::uniform_real_distribution<float>(0.0f, 1.0f)(shulkerRandom());
    if ((float) (crowd - 1) / CROWD_DIVISOR > chance)
        return;
    owner.spawnActor(owner.getLevelFor(*this), IDENTIFIER, at);
}

void ShulkerActor::_setPeek(ServerNetworkHandler &owner, int32_t peek) {
    if (mPeek == peek)
        return;
    mPeek = peek;
    _sync(owner, PEEK_DATA_ID);
}

void ShulkerActor::_setAttachFace(ServerNetworkHandler &owner, int8_t face) {
    if (mAttachFace == face)
        return;
    mAttachFace = face;
    _sync(owner, ATTACH_FACE_DATA_ID);
}

void ShulkerActor::_setAttachPosition(ServerNetworkHandler &owner, const Vector3i &position) {
    const bool moved = !sameBlock(mAttachPosition, position);
    mAttachPosition = position;
    if (moved)
        _sync(owner, ATTACH_POSITION_DATA_ID);
    if (!mAttached) {
        mAttached = true;
        _sync(owner, ATTACHED_DATA_ID);
    }
}

void ShulkerActor::_sync(ServerNetworkHandler &owner, int32_t dataId) {
    EntityDataMap all;
    fillSpawnMetadata(all);
    EntityDataMap changed;
    for (const EntityDataEntry &entry: all.mEntries) {
        if (entry.mId == dataId)
            changed.mEntries.push_back(entry);
    }
    owner.sendActorMetadata(*this, changed);
}

void ShulkerActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    HostileActor::fillSpawnMetadata(metadata);

    EntityDataEntry peek;
    peek.mId = PEEK_DATA_ID;
    peek.mFormat = EntityDataFormat::Int;
    peek.mIntValue = mPeek;
    metadata.mEntries.push_back(peek);

    EntityDataEntry face;
    face.mId = ATTACH_FACE_DATA_ID;
    face.mFormat = EntityDataFormat::Byte;
    face.mByteValue = mAttachFace;
    metadata.mEntries.push_back(face);

    EntityDataEntry attached;
    attached.mId = ATTACHED_DATA_ID;
    attached.mFormat = EntityDataFormat::Short;
    attached.mShortValue = mAttached ? 1 : 0;
    metadata.mEntries.push_back(attached);

    EntityDataEntry position;
    position.mId = ATTACH_POSITION_DATA_ID;
    position.mFormat = EntityDataFormat::Vector3i;
    position.mVector3iValue = mAttachPosition;
    metadata.mEntries.push_back(position);
}

// the game's keys: the block as a list of three ints, the face and the shell's opening as bytes
Tag ShulkerActor::saveNbt() const {
    Tag data = HostileActor::saveNbt();
    if (mAttached) {
        Tag position = Tag::ofList(Tag::Type::Int);
        position.addToList(Tag::ofInt(mAttachPosition.x));
        position.addToList(Tag::ofInt(mAttachPosition.y));
        position.addToList(Tag::ofInt(mAttachPosition.z));
        data.put(TAG_ATTACH_POSITION, position);
    }
    data.putByte(TAG_ATTACH_FACE, mAttachFace);
    data.putByte(TAG_PEEK, (int8_t) mPeek);
    return data;
}

void ShulkerActor::loadNbt(const Tag &data) {
    HostileActor::loadNbt(data);
    const Tag *position = data.get(TAG_ATTACH_POSITION);
    if (position != nullptr && position->isList() && position->getList().size() >= 3) {
        const std::vector<Tag> &values = position->getList();
        mAttachPosition = Vector3i(values[0].asInt(), values[1].asInt(), values[2].asInt());
        mAttached = true;
    }
    const int8_t face = data.getByte(TAG_ATTACH_FACE, FACE_DOWN);
    mAttachFace = face >= 0 && face < FACE_COUNT ? face : FACE_DOWN;
    mPeek = (uint8_t) data.getByte(TAG_PEEK, PEEK_CLOSED);
}
