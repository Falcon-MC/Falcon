#include "Actor/AI/Goal/FollowOwnerGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const int32_t REPATH_INTERVAL = 10;
}

FollowOwnerGoal::FollowOwnerGoal(float speed, float startDistance, float stopDistance)
        : mSpeed(speed), mStartDistance(startDistance), mStopDistance(stopDistance) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool FollowOwnerGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.isSitting())
        return false;

    const ServerPlayer *player = mob.getOwner(owner);
    return player != nullptr && mob.distanceSquaredTo(*player) > mStartDistance * mStartDistance;
}

bool FollowOwnerGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.isSitting())
        return false;

    const ServerPlayer *player = mob.getOwner(owner);
    return player != nullptr && mob.distanceSquaredTo(*player) > mStopDistance * mStopDistance;
}

void FollowOwnerGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mTicksUntilRepath = 0;
}

void FollowOwnerGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mob.getLookControl().clear();
}

void FollowOwnerGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *player = mob.getOwner(owner);
    if (player == nullptr)
        return;

    mob.getLookControl().setLookAt(player->getPosition());
    if (--mTicksUntilRepath > 0)
        return;

    mTicksUntilRepath = REPATH_INTERVAL;
    mob.getNavigation().moveTo(player->getPosition(), mSpeed);
}
