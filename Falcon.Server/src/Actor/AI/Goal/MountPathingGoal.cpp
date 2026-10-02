#include "Actor/AI/Goal/MountPathingGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/RideSystem.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const int32_t REPATH_INTERVAL = 10;
}

MountPathingGoal::MountPathingGoal(float speed, float targetDistance, bool trackTarget)
        : mSpeed(speed), mTargetDistance(targetDistance), mTrackTarget(trackTarget) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

Actor *MountPathingGoal::_riderTarget(ServerNetworkHandler &owner, const MobActor &mob) {
    const std::vector<int64_t> &passengers = mob.getPassengers();
    if (passengers.empty())
        return nullptr;

    const MobActor *rider = dynamic_cast<const MobActor *>(RideSystem::resolve(owner, passengers.front()));
    return rider == nullptr ? nullptr : rider->getTarget(owner);
}

bool MountPathingGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = _riderTarget(owner, mob);
    return target != nullptr && mob.distanceSquaredTo(*target) > mTargetDistance * mTargetDistance;
}

bool MountPathingGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return canUse(owner, mob) && (mTrackTarget || !mob.getNavigation().isDone());
}

void MountPathingGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mRepathTicks = 0;
    const Actor *target = _riderTarget(owner, mob);
    if (target != nullptr)
        mob.getNavigation().moveTo(target->getPosition(), mSpeed);
}

void MountPathingGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}

void MountPathingGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = _riderTarget(owner, mob);
    if (target == nullptr)
        return;

    mob.getLookControl().setLookAt(target->getPosition());
    if (!mTrackTarget || --mRepathTicks > 0)
        return;

    mRepathTicks = REPATH_INTERVAL;
    mob.getNavigation().moveTo(target->getPosition(), mSpeed);
}
