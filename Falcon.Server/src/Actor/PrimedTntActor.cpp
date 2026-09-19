#include "Actor/PrimedTntActor.h"

#include "Actor/ActorFlags.h"
#include "Level/Explosion.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <vector>

const char *PrimedTntActor::IDENTIFIER = "minecraft:tnt";

const float PrimedTntActor::GRAVITY = 0.04f;
const float PrimedTntActor::DRAG = 0.02f;
const float PrimedTntActor::SIZE = 0.98f;
const double PrimedTntActor::EXPLOSION_Y_OFFSET = 0.06125;
const double PrimedTntActor::EXPLOSION_SIZE = 4.0;

namespace {
    const char *TAG_FUSE = "Fuse";
}

PrimedTntActor::PrimedTntActor(uint64_t runtimeId, int32_t fuse)
        : ServerActor(runtimeId, IDENTIFIER), mFuse(fuse) {
    getFlags().set(ActorFlag::Ignited, true);
    getFlags().set(ActorFlag::HasGravity, true);
    getFlags().set(ActorFlag::HasCollision, true);
}

void PrimedTntActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    EntityDataEntry flags;
    flags.mId = ActorFlags::FLAGS_DATA_ID;
    flags.mFormat = EntityDataFormat::Long;
    flags.mLongValue = getFlags().getLowBits();
    metadata.mEntries.push_back(flags);

    EntityDataEntry flags2;
    flags2.mId = ActorFlags::FLAGS_2_DATA_ID;
    flags2.mFormat = EntityDataFormat::Long;
    flags2.mLongValue = getFlags().getHighBits();
    metadata.mEntries.push_back(flags2);

    metadata.mEntries.push_back(_fuseEntry());
}

EntityDataEntry PrimedTntActor::_fuseEntry() const {
    EntityDataEntry fuse;
    fuse.mId = ActorFlags::FUSE_LENGTH_DATA_ID;
    fuse.mFormat = EntityDataFormat::Int;
    fuse.mIntValue = mFuse;
    return fuse;
}

void PrimedTntActor::tick(ServerNetworkHandler &owner) {
    if (mExpired)
        return;

    Level &level = owner.getLevelFor(*this);
    Vector3f position = getPosition();
    if (!level.isChunkResident((int32_t) std::floor(position.x) >> 4, (int32_t) std::floor(position.z) >> 4))
        return;

    if (mFuse % FUSE_SYNC_INTERVAL == 0) {
        EntityDataMap metadata;
        metadata.mEntries.push_back(_fuseEntry());
        owner.sendActorMetadata(*this, metadata);
    }

    Vector3f motion = getMotion();
    motion.y -= GRAVITY;

    const float halfWidth = SIZE * 0.5f;
    AxisAlignedBB box(position.x - halfWidth, position.y, position.z - halfWidth,
                      position.x + halfWidth, position.y + SIZE, position.z + halfWidth);
    const std::vector<AxisAlignedBB> colliders = level.getCollisionBoxes(box.addCoord(motion.x, motion.y, motion.z));

    float moveY = motion.y;
    for (const AxisAlignedBB &collider: colliders)
        moveY = collider.calculateYOffset(box, moveY);
    box = box.offset(0.0f, moveY, 0.0f);

    float moveX = motion.x;
    for (const AxisAlignedBB &collider: colliders)
        moveX = collider.calculateXOffset(box, moveX);
    box = box.offset(moveX, 0.0f, 0.0f);

    float moveZ = motion.z;
    for (const AxisAlignedBB &collider: colliders)
        moveZ = collider.calculateZOffset(box, moveZ);
    box = box.offset(0.0f, 0.0f, moveZ);

    const bool onGround = motion.y < 0.0f && moveY != motion.y;
    if (moveX != motion.x)
        motion.x = 0.0f;
    if (moveZ != motion.z)
        motion.z = 0.0f;
    if (moveY != motion.y && !onGround)
        motion.y = 0.0f;

    position = Vector3f(box.mMinX + halfWidth, box.mMinY, box.mMinZ + halfWidth);

    const float friction = 1.0f - DRAG;
    motion.x *= friction;
    motion.y *= friction;
    motion.z *= friction;

    if (onGround) {
        motion.y *= -0.5f;
        motion.x *= 0.7f;
        motion.z *= 0.7f;
    }

    if (position.y < (float) level.getMinY()) {
        mExpired = true;
        return;
    }

    setMotion(motion);
    setPosition(position);
    setOnGround(onGround);
    owner.broadcastActorMove(*this);

    --mFuse;
    if (mFuse <= 0) {
        _explode(owner);
        mExpired = true;
    }
}

void PrimedTntActor::_explode(ServerNetworkHandler &owner) {
    if (!owner.getLevel().getGameRules().getBool("tntexplodes"))
        return;

    const Vector3f position = getPosition();
    const Vector3f center(position.x, (float) ((double) position.y + EXPLOSION_Y_OFFSET), position.z);

    Explosion explosion(owner, owner.getLevelFor(*this), center, EXPLOSION_SIZE, this, true);
    explosion.explode();
}

Tag PrimedTntActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();
    data.putByte(TAG_FUSE, (int8_t) mFuse);
    return data;
}

void PrimedTntActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);
    mFuse = data.getByte(TAG_FUSE, (int8_t) DEFAULT_FUSE);
}
