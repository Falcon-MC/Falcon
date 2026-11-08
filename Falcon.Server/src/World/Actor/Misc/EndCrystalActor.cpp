#include "World/Actor/Misc/EndCrystalActor.h"

#include "World/Actor/ActorClassRegistry.h"
#include "World/Level/Dimension/EndDragonFight.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "Network/ServerNetworkHandler.h"

FALCON_REGISTER_ACTOR(EndCrystalActor, EndCrystalActor::IDENTIFIER);

namespace {
    const char *const INVULNERABLE_TAG = "Invulnerable";
    const char *const BLOCK_TARGET_X_TAG = "BlockTargetX";
    const char *const BLOCK_TARGET_Y_TAG = "BlockTargetY";
    const char *const BLOCK_TARGET_Z_TAG = "BlockTargetZ";
}

EndCrystalActor::EndCrystalActor(uint64_t runtimeId, const std::string &identifier)
        : ServerActor(runtimeId, identifier) {
}

void EndCrystalActor::tick(ServerNetworkHandler &owner) {
    (void) owner;
}

void EndCrystalActor::setShowBottom(bool showBottom) {
    getFlags().set(ActorFlag::ShowBottom, showBottom);
}

void EndCrystalActor::_appendBeamTarget(EntityDataMap &metadata) const {
    EntityDataEntry target;
    target.mId = BLOCK_TARGET_DATA_ID;
    target.mFormat = EntityDataFormat::Vector3i;
    // the game sends the origin when the crystal has no beam
    target.mVector3iValue = mBeamTarget.has_value() ? *mBeamTarget : Vector3i(0, 0, 0);
    metadata.mEntries.push_back(target);
}

void EndCrystalActor::setBeamTarget(ServerNetworkHandler &owner, const std::optional<Vector3i> &target) {
    if (mBeamTarget == target)
        return;

    mBeamTarget = target;
    EntityDataMap metadata;
    _appendBeamTarget(metadata);
    owner.sendActorMetadata(*this, metadata);
}

void EndCrystalActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    const int32_t flagIds[] = {ActorFlags::FLAGS_DATA_ID, ActorFlags::FLAGS_2_DATA_ID};
    const int64_t flagValues[] = {getFlags().getLowBits(), getFlags().getHighBits()};
    for (int index = 0; index < 2; index++) {
        EntityDataEntry entry;
        entry.mId = flagIds[index];
        entry.mFormat = EntityDataFormat::Long;
        entry.mLongValue = flagValues[index];
        metadata.mEntries.push_back(entry);
    }

    if (mBeamTarget.has_value())
        _appendBeamTarget(metadata);
}

bool EndCrystalActor::onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) {
    (void) amount;

    if (mExpired)
        return true;

    mExpired = true;

    Level &level = owner.getLevelFor(*this);

    Explosion explosion(owner, level, getPosition(), EXPLOSION_SIZE, this, false);
    explosion.explode();

    if (level.getDimensionType() == DimensionType::TheEnd)
        owner.getWorldFor(*this).getDragonFight().onCrystalDestroyed(owner, level, *this, (Actor *) source);
    return true;
}

Tag EndCrystalActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();
    data.putBool(SHOW_BOTTOM_TAG, getFlags().get(ActorFlag::ShowBottom));
    data.putBool(INVULNERABLE_TAG, mInvulnerable);
    if (mBeamTarget.has_value()) {
        data.putInt(BLOCK_TARGET_X_TAG, mBeamTarget->x);
        data.putInt(BLOCK_TARGET_Y_TAG, mBeamTarget->y);
        data.putInt(BLOCK_TARGET_Z_TAG, mBeamTarget->z);
    }
    return data;
}

void EndCrystalActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);
    setShowBottom(data.getBool(SHOW_BOTTOM_TAG));
    mInvulnerable = data.getBool(INVULNERABLE_TAG);
    if (data.contains(BLOCK_TARGET_X_TAG)) {
        mBeamTarget = Vector3i(data.getInt(BLOCK_TARGET_X_TAG), data.getInt(BLOCK_TARGET_Y_TAG),
                               data.getInt(BLOCK_TARGET_Z_TAG));
    }
}
