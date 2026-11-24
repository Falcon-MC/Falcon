#include "World/Actor/AI/Goal/MoveTowardsTargetGoal.h"

#include "World/Actor/AI/Goal/RandomGroundPosition.h"
#include "World/Actor/Mob/MobActor.h"
#include "Network/ServerNetworkHandler.h"

#include <random>

namespace {
    // the game's fixed search box for the point towards the target
    const int32_t SEARCH_HORIZONTAL = 16;
    const int32_t SEARCH_VERTICAL = 7;

    std::mt19937 &towardsRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

MoveTowardsTargetGoal::MoveTowardsTargetGoal(float speed, float withinRadius)
        : mSpeed(speed), mWithinRadius(withinRadius) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool MoveTowardsTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!_isTargetInRange(owner, mob, false))
        return false;

    const Actor *target = mob.getTarget(owner);
    const Vector3f direction(target->getPosition().x - mob.getPosition().x,
                             target->getPosition().y - mob.getPosition().y,
                             target->getPosition().z - mob.getPosition().z);
    Vector3i wanted;
    if (!RandomGroundPosition::find(owner.getLevelFor(mob), mob, SEARCH_HORIZONTAL, SEARCH_VERTICAL, &direction,
                                    towardsRandom(), wanted))
        return false;

    mWanted = Vector3f((float) wanted.x, (float) wanted.y, (float) wanted.z);
    return true;
}

bool MoveTowardsTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return !mob.getNavigation().isDone() && _isTargetInRange(owner, mob, true);
}

void MoveTowardsTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().moveTo(mWanted, mSpeed);
}

bool MoveTowardsTargetGoal::_isTargetInRange(ServerNetworkHandler &owner, const MobActor &mob, bool strict) const {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr || !target->isAlive())
        return false;

    // the radius itself starts the goal but ends it
    const float distance = mob.distanceSquaredTo(*target);
    const float limit = mWithinRadius * mWithinRadius;
    return strict ? distance < limit : distance <= limit;
}
