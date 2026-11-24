#include "World/Actor/Misc/PrimedTntActor.h"

#include "World/Actor/ActorFlags.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Network/ServerNetworkHandler.h"

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
    const float FAR_MOVE = 3.0f;
}

PrimedTntActor::PrimedTntActor(uint64_t runtimeId, int32_t fuse) : ServerActor(runtimeId, IDENTIFIER), mFuse(fuse) {
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


    EntityDataEntry flags3;

    flags3.mId = ActorFlags::FLAGS_3_DATA_ID;

    flags3.mFormat = EntityDataFormat::Long;

    flags3.mLongValue = getFlags().getExtendedBits();
metadata.mEntries.push_back(flags3);


    EntityDataEntry nameplateDistance;

    nameplateDistance.mId = ActorFlags::NAMEPLATE_RENDER_DISTANCE_DATA_ID;

    nameplateDistance.mFormat = EntityDataFormat::Float;

    nameplateDistance.mFloatValue = getNameplateRenderDistance();

    metadata.mEntries.push_back(nameplateDistance);

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
    AxisAlignedBB box(position.x - halfWidth, position.y, position.z - halfWidth, position.x + halfWidth,
                      position.y + SIZE, position.z + halfWidth);
    // a blast throws it far: each axis then asks for its own path, not for the whole box around the move
    const bool farMove = std::fabs(motion.x) + std::fabs(motion.y) + std::fabs(motion.z) > FAR_MOVE;
    // through open sky or water nothing is in the way: known from the chunk sections, no block is read
    const bool freeMove = farMove && level.isFreeOfColliders(box.addCoord(motion.x, motion.y, motion.z), this);
    std::vector<AxisAlignedBB> colliders;
    if (!freeMove)
        colliders = level.getCollisionBoxes(
                farMove ? box.addCoord(0.0f, motion.y, 0.0f) : box.addCoord(motion.x, motion.y, motion.z), this);

    float moveY = motion.y;
    for (const AxisAlignedBB &collider: colliders)
        moveY = collider.calculateYOffset(box, moveY);
    box = box.offset(0.0f, moveY, 0.0f);

    if (farMove && !freeMove)
        colliders = level.getCollisionBoxes(box.addCoord(motion.x, 0.0f, 0.0f), this);
    float moveX = motion.x;
    for (const AxisAlignedBB &collider: colliders)
        moveX = collider.calculateXOffset(box, moveX);
    box = box.offset(moveX, 0.0f, 0.0f);

    if (farMove && !freeMove)
        colliders = level.getCollisionBoxes(box.addCoord(0.0f, 0.0f, motion.z), this);
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

    // a tnt lying still has nothing new to tell its viewers
    const Vector3f previous = getPosition();
    setMotion(motion);
    setPosition(position);
    setOnGround(onGround);
    if (position.x != previous.x || position.y != previous.y || position.z != previous.z)
        owner.broadcastActorMove(*this);

    --mFuse;
    if (mFuse <= 0) {
        _explode(owner);
        mExpired = true;
    }
}

void PrimedTntActor::_explode(ServerNetworkHandler &owner) {
    if (!owner.getWorldFor(*this).getOverworld().getGameRules().getBool("tntexplodes"))
        return;

    const Vector3f position = getPosition();
    const Vector3f center(position.x, (float) ((double) position.y + EXPLOSION_Y_OFFSET), position.z);

    owner.explodeAfterActors(owner.getLevelFor(*this), center, EXPLOSION_SIZE, *this, true);
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
