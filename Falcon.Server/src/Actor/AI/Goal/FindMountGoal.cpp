#include "Actor/AI/Goal/FindMountGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/RideSystem.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>

namespace {
    const char *const RIDEABLE_COMPONENT = "minecraft:rideable";
    const float MOUNT_DISTANCE_SQUARED = 4.0f;
    const int32_t REPATH_INTERVAL = 10;
}

FindMountGoal::FindMountGoal(float speed, float radius, int32_t startDelay, int32_t maxFailedAttempts)
        : mSpeed(speed), mRadius(radius), mStartDelay(startDelay), mMaxFailedAttempts(maxFailedAttempts) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool FindMountGoal::_accepts(const MobActor &vehicle, const MobActor &rider) {
    const json::Value *rideable = vehicle.getComponent(RIDEABLE_COMPONENT);
    if (rideable == nullptr || !vehicle.isAlive() || vehicle.isRiding())
        return false;

    const json::Value *seats = rideable->get("seat_count");
    const size_t seatCount = (size_t) std::max(1, seats == nullptr ? 1 : seats->integer(1));
    if (vehicle.getPassengers().size() >= seatCount)
        return false;

    const json::Value *families = rideable->get("family_types");
    if (families == nullptr || !families->isArray())
        return true;

    for (const std::string &family: rider.getFamilies()) {
        for (const std::unique_ptr<json::Value> &allowed: families->mArray) {
            if (allowed->string() == family)
                return true;
        }
    }
    return false;
}

MobActor *FindMountGoal::_findMount(ServerNetworkHandler &owner, const MobActor &mob) const {
    MobActor *nearest = nullptr;
    float nearestDistance = mRadius * mRadius;
    for (auto &entry: owner.getActors()) {
        MobActor *candidate = dynamic_cast<MobActor *>(entry.second.get());
        if (candidate == nullptr || candidate == &mob || candidate->getDimension() != mob.getDimension()
            || !_accepts(*candidate, mob))
            continue;

        const float distance = mob.distanceSquaredTo(*candidate);
        if (distance <= nearestDistance) {
            nearest = candidate;
            nearestDistance = distance;
        }
    }
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
    return !mob.isRiding() && mount != nullptr && _accepts(*mount, mob);
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
