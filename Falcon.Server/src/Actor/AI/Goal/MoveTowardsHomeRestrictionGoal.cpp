#include "Actor/AI/Goal/MoveTowardsHomeRestrictionGoal.h"

#include "Actor/Mob/MobActor.h"

MoveTowardsHomeRestrictionGoal::MoveTowardsHomeRestrictionGoal(float speed, float radius)
        : mSpeed(speed), mRadius(radius) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool MoveTowardsHomeRestrictionGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mob.hasHome() && _isOutside(mob);
}

bool MoveTowardsHomeRestrictionGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mob.hasHome() && _isOutside(mob) && !mob.getNavigation().isDone();
}

void MoveTowardsHomeRestrictionGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().moveTo(mob.getHomePosition(), mSpeed);
}

void MoveTowardsHomeRestrictionGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}

bool MoveTowardsHomeRestrictionGoal::_isOutside(const MobActor &mob) const {
    const Vector3f position = mob.getPosition();
    const Vector3f &home = mob.getHomePosition();
    const float dx = position.x - home.x;
    const float dy = position.y - home.y;
    const float dz = position.z - home.z;
    return dx * dx + dy * dy + dz * dz > mRadius * mRadius;
}
