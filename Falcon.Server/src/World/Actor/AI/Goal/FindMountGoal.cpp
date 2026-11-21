#include "World/Actor/AI/Goal/FindMountGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/RideSystem.h"
#include "Network/ServerNetworkHandler.h"

namespace {
    const float MOUNT_DISTANCE_SQUARED = 4.0f;
    const int32_t REPATH_INTERVAL = 10;
}

FindMountGoal::FindMountGoal(float speed, float radius, int32_t startDelay, int32_t maxFailedAttempts)
        : mSpeed(speed), mRadius(radius), mStartDelay(startDelay), mMaxFailedAttempts(maxFailedAttempts) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

MobActor *FindMountGoal::_findMount(ServerNetworkHandler &owner, const MobActor &mob) const {
    MobActor *nearest = nullptr;
    float nearestDistance = mRadius * mRadius;
    owner.forEachActorNear(mob.getPosition(), mRadius, [&](ServerActor &actor) {
        MobActor *candidate = dynamic_cast<MobActor *>(&actor);
        if (candidate == nullptr || candidate == &mob || !candidate->sharesLevelWith(mob)
            || !candidate->acceptsRider(mob))
            return;

        const float distance = mob.distanceSquaredTo(*candidate);
        if (distance <= nearestDistance) {
            nearest = candidate;
            nearestDistance = distance;
        }
    });
    return nearest;
}

MobActor *FindMountGoal::_currentMount(ServerNetworkHandler &owner) const {
    return dynamic_cast<MobActor *>(RideSystem::resolve(owner, mMountId));
}

bool FindMountGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.isRiding() || (mMaxFailedAttempts > 0 && mFailedAttempts >= mMaxFailedAttempts))
        return false;

    if (mDelay < mStartDelay) {
        ++mDelay;
        return false;
    }

    const MobActor *mount = _findMount(owner, mob);
    if (mount == nullptr)
        return false;

    mMountId = mount->getUniqueId();
    return true;
}

bool FindMountGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const MobActor *mount = _currentMount(owner);
    return !mob.isRiding() && mount != nullptr && mount->acceptsRider(mob);
}

void FindMountGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mRepathTicks = 0;
    mPathIssued = false;
}

void FindMountGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mMountId = 0;
    mob.getNavigation().stop(mob);
}

void FindMountGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    MobActor *mount = _currentMount(owner);
    if (mount == nullptr)
        return;

    if (mob.distanceSquaredTo(*mount) <= MOUNT_DISTANCE_SQUARED) {
        if (!RideSystem::mount(owner, mob, *mount, false))
            ++mFailedAttempts;
        return;
    }

    if (--mRepathTicks > 0)
        return;

    if (mPathIssued && mob.getNavigation().isDone())
        ++mFailedAttempts;

    mPathIssued = true;
    mRepathTicks = REPATH_INTERVAL;
    mob.getNavigation().moveTo(mount->getPosition(), mSpeed);
}
