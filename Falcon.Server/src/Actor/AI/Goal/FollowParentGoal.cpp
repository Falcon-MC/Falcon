#include "Actor/AI/Goal/FollowParentGoal.h"

#include "Actor/ActorFlags.h"
#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <cstring>

namespace {
    const float SEARCH_HORIZONTAL = 8.0f;
    const float SEARCH_VERTICAL = 4.0f;
    const float CLOSE_DISTANCE_SQUARED = 9.0f;
    const float LOST_DISTANCE_SQUARED = 256.0f;
    const int32_t REPATH_INTERVAL = 10;

    bool isBaby(const MobActor &mob) {
        return mob.getFlags().get(ActorFlag::Baby);
    }
}

FollowParentGoal::FollowParentGoal(float speed) : mSpeed(speed) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool FollowParentGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!isBaby(mob))
        return false;

    const MobActor *parent = _findParent(owner, mob);
    if (parent == nullptr || mob.distanceSquaredTo(*parent) < CLOSE_DISTANCE_SQUARED)
        return false;

    mParentId = parent->getUniqueId();
    return true;
}

bool FollowParentGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const MobActor *parent = dynamic_cast<const MobActor *>(owner.getActor(mParentId));
    if (!isBaby(mob) || parent == nullptr || !parent->isAlive())
        return false;

    const float distance = mob.distanceSquaredTo(*parent);
    return distance >= CLOSE_DISTANCE_SQUARED && distance <= LOST_DISTANCE_SQUARED;
}

void FollowParentGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mTicksUntilRepath = 0;
}

void FollowParentGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mParentId = 0;
    mob.getNavigation().stop(mob);
}

void FollowParentGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (--mTicksUntilRepath > 0)
        return;

    mTicksUntilRepath = REPATH_INTERVAL;
    const ServerActor *parent = owner.getActor(mParentId);
    if (parent != nullptr)
        mob.getNavigation().moveTo(parent->getPosition(), mSpeed);
}

MobActor *FollowParentGoal::_findParent(ServerNetworkHandler &owner, const MobActor &mob) const {
    MobActor *nearest = nullptr;
    float nearestDistance = SEARCH_HORIZONTAL * SEARCH_HORIZONTAL + SEARCH_VERTICAL * SEARCH_VERTICAL;
    const Vector3f position = mob.getPosition();

    for (auto &entry: owner.getActors()) {
        MobActor *candidate = dynamic_cast<MobActor *>(entry.second.get());
        if (candidate == nullptr || candidate == &mob || !candidate->isAlive() || isBaby(*candidate)
            || candidate->getDimension() != mob.getDimension()
            || std::strcmp(candidate->getIdentifier(), mob.getIdentifier()) != 0)
            continue;

        const Vector3f other = candidate->getPosition();
        if (std::fabs(other.x - position.x) > SEARCH_HORIZONTAL || std::fabs(other.z - position.z) > SEARCH_HORIZONTAL
            || std::fabs(other.y - position.y) > SEARCH_VERTICAL)
            continue;

        const float distance = mob.distanceSquaredTo(*candidate);
        if (distance < nearestDistance) {
            nearest = candidate;
            nearestDistance = distance;
        }
    }
    return nearest;
}
