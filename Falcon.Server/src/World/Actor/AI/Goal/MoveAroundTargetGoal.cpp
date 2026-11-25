#include "World/Actor/AI/Goal/MoveAroundTargetGoal.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Goal/JumpAroundTargetGoal.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"

#include <utility>

MoveAroundTargetGoal::MoveAroundTargetGoal(Settings settings)
        : mSettings(std::move(settings)), mRandom(std::random_device{}()) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool MoveAroundTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getTarget(owner) == nullptr)
        return false;
    return mSettings.mFilters == nullptr || EntityFilter::test(*mSettings.mFilters, owner, mob, &mob);
}

bool MoveAroundTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return !mob.getNavigation().isDone();
}

void MoveAroundTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;
    // the spot is drawn as the jump draws its landing
    const Vector3f destination = JumpAroundTargetGoal::positionAroundTarget(
            *target, mSettings.mSpreadDegrees, mSettings.mMinDistance, mSettings.mMaxDistance, mRandom);
    mob.getNavigation().moveTo(destination, mSettings.mSpeed);
}

void MoveAroundTargetGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}
