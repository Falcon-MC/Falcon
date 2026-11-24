#include "World/Actor/AI/Goal/FireAtTargetGoal.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Control/LookControl.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/Component/MobShooter.h"
#include "World/Actor/Mob/MobActor.h"

#include <cmath>
#include <utility>

namespace {
    const char *const SHOOT_SOUND = "shoot";
    const float DEGREES_TO_RADIANS = 0.017453292f;
    const float PLAYER_WIDTH = 0.6f;
    const float PLAYER_HEIGHT = 1.8f;
    const float EYE_HEIGHT_RATIO = 0.85f;

    ActorSize sizeOf(const Actor &actor) {
        const ServerActor *server = dynamic_cast<const ServerActor *>(&actor);
        return server == nullptr ? ActorSize{PLAYER_WIDTH, PLAYER_HEIGHT} : server->getSize();
    }

    float distanceSquared(const Vector3f &a, const Vector3f &b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    void setFacing(ServerNetworkHandler &owner, MobActor &mob, bool facing) {
        if (mob.getFlags().get(ActorFlag::FacingTargetToRangeAttack) == facing)
            return;
        mob.getFlags().set(ActorFlag::FacingTargetToRangeAttack, facing);
        owner.syncActorFlags(mob);
    }

    Actor *liveTarget(ServerNetworkHandler &owner, const MobActor &mob) {
        Actor *target = mob.getTarget(owner);
        return target != nullptr && target->isAlive() ? target : nullptr;
    }
}

FireAtTargetGoal::FireAtTargetGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Look);
}

Vector3f FireAtTargetGoal::anchorPoint(const Actor &actor, Anchor anchor, const Vector3f &offset) {
    const ActorSize size = sizeOf(actor);
    Vector3f point = actor.getPosition();
    if (anchor == Anchor::Middle)
        point.y += size.mHeight * 0.5f;
    else if (anchor == Anchor::Eyes)
        point.y += size.mHeight * EYE_HEIGHT_RATIO;

    // the box is as deep as it is wide
    const float yaw = actor.getRotation().y * -DEGREES_TO_RADIANS;
    const float cosine = std::cos(yaw);
    const float sine = std::sin(yaw);
    const float x = offset.x * size.mWidth;
    const float z = offset.z * size.mWidth;
    point.x += z * sine + x * cosine;
    point.y += offset.y * size.mHeight;
    point.z += z * cosine - x * sine;
    return point;
}

bool FireAtTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (owner.getCurrentTick() < mCooldownEnd || mSettings.mProjectile.empty())
        return false;
    if (mSettings.mFilters != nullptr && !EntityFilter::test(*mSettings.mFilters, owner, mob, &mob))
        return false;

    const Actor *target = liveTarget(owner, mob);
    if (target == nullptr)
        return false;
    const float distance = distanceSquared(target->getPosition(), mob.getPosition());
    return mSettings.mMinRangeSquared < distance && distance < mSettings.mMaxRangeSquared;
}

bool FireAtTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mob.getTarget(owner) != nullptr && owner.getCurrentTick() <= mEndTick;
}

void FireAtTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    const int64_t now = owner.getCurrentTick();
    mShootTick = now + mSettings.mPreShootDelay;
    mEndTick = now + mSettings.mPreShootDelay + mSettings.mPostShootDelay;
    setFacing(owner, mob, true);
}

void FireAtTargetGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    mShootTick = 0;
    mFired = false;
    mCooldownEnd = owner.getCurrentTick() + mSettings.mCooldown;
    setFacing(owner, mob, false);
}

void FireAtTargetGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = liveTarget(owner, mob);
    if (target == nullptr)
        return;

    // the body turns to the target while the head follows it
    Vector3f rotation = mob.getRotation();
    rotation.y = LookControl::yawTowards(mob.getPosition(), target->getPosition());
    mob.setRotation(rotation);
    const Vector3f targetPosition = target->getPosition();
    mob.getLookControl().setLookAt(
            Vector3f(targetPosition.x, targetPosition.y + sizeOf(*target).mHeight * EYE_HEIGHT_RATIO, targetPosition.z));

    if (mFired || owner.getCurrentTick() < mShootTick)
        return;
    mFired = true;
    if (!mob.canSee(owner, *target))
        return;

    const Vector3f from = anchorPoint(mob, mSettings.mOwnerAnchor, mSettings.mOwnerOffset);
    const Vector3f to = anchorPoint(*target, mSettings.mTargetAnchor, mSettings.mTargetOffset);
    if (MobShooter::shootFrom(owner, mob, mSettings.mProjectile, from, Vector3f(to.x - from.x, to.y - from.y, to.z - from.z))
        != nullptr)
        owner.playLevelSound(owner.getLevelFor(mob), SHOOT_SOUND, mob.getPosition(), mob.getIdentifier());
}
