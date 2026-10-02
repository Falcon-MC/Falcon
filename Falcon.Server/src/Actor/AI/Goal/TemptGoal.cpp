#include "Actor/AI/Goal/TemptGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <utility>

namespace {
    const int32_t REPATH_INTERVAL = 10;
}

TemptGoal::TemptGoal(float speed, float range, BehaviorItems items, float stopDistance)
        : mSpeed(speed), mRange(range), mItems(std::move(items)), mStopDistanceSquared(stopDistance * stopDistance) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool TemptGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return !mItems.isEmpty() && mItems.findNearestHolder(owner, mob, mRange) != nullptr;
}

void TemptGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mTicksUntilRepath = 0;
    mob.getLookControl().setPitchEnabled(true);
}

void TemptGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mob.getLookControl().clear();
    mob.getLookControl().setPitchEnabled(false);
}

void TemptGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *tempter = mItems.findNearestHolder(owner, mob, mRange);
    if (tempter == nullptr)
        return;

    mob.getLookControl().setLookAt(tempter->getPosition());
    if (mob.distanceSquaredTo(*tempter) <= mStopDistanceSquared) {
        mob.getNavigation().stop(mob);
        return;
    }

    if (--mTicksUntilRepath > 0)
        return;

    mTicksUntilRepath = REPATH_INTERVAL;
    mob.getNavigation().moveTo(tempter->getPosition(), mSpeed);
}
