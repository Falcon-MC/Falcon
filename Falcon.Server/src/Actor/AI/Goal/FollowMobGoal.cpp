#include "Actor/AI/Goal/FollowMobGoal.h"

#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/RideSystem.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <utility>

namespace {
    const int32_t REPATH_INTERVAL = 10;
}

FollowMobGoal::FollowMobGoal(float speed, float searchRange, float stopDistance,
                             std::shared_ptr<json::Value> filters)
        : mSpeed(speed), mSearchRange(searchRange), mStopDistance(stopDistance), mFilters(std::move(filters)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool FollowMobGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *leader = _findLeader(owner, mob);
    if (leader == nullptr)
        return false;

    mLeaderId = leader->getUniqueId();
    return true;
}

bool FollowMobGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *leader = _resolveLeader(owner);
    if (leader == nullptr || !leader->isAlive() || leader->getDimension() != mob.getDimension())
        return false;

    return mob.distanceSquaredTo(*leader) <= mSearchRange * mSearchRange;
}

void FollowMobGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mTicksUntilRepath = 0;
}

void FollowMobGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mLeaderId = 0;
    mob.getNavigation().stop(mob);
    mob.getLookControl().clear();
}

void FollowMobGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *leader = _resolveLeader(owner);
    if (leader == nullptr)
        return;

    mob.getLookControl().setLookAt(leader->getPosition());
    if (mob.distanceSquaredTo(*leader) <= mStopDistance * mStopDistance) {
        mob.getNavigation().stop(mob);
        return;
    }

    if (--mTicksUntilRepath > 0)
        return;

    mTicksUntilRepath = REPATH_INTERVAL;
    mob.getNavigation().moveTo(leader->getPosition(), mSpeed);
}

Actor *FollowMobGoal::_resolveLeader(ServerNetworkHandler &owner) const {
    return RideSystem::resolve(owner, mLeaderId);
}

bool FollowMobGoal::_accepts(ServerNetworkHandler &owner, const MobActor &mob, const Actor &candidate) const {
    if (&candidate == &mob || !candidate.isAlive() || candidate.getDimension() != mob.getDimension())
        return false;

    if (mob.distanceSquaredTo(candidate) > mSearchRange * mSearchRange)
        return false;

    return mFilters == nullptr || EntityFilter::test(*mFilters, owner, mob, &candidate);
}

Actor *FollowMobGoal::_findLeader(ServerNetworkHandler &owner, const MobActor &mob) const {
    Actor *nearest = nullptr;
    float nearestDistance = mSearchRange * mSearchRange;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.getGameType() == (int32_t) GameType::Spectator
            || !_accepts(owner, mob, player))
            continue;

        const float distance = mob.distanceSquaredTo(player);
        if (distance <= nearestDistance) {
            nearest = &player;
            nearestDistance = distance;
        }
    }

    for (auto &entry: owner.getActors()) {
        ServerActor *candidate = entry.second.get();
        if (dynamic_cast<MobActor *>(candidate) == nullptr || !_accepts(owner, mob, *candidate))
            continue;

        const float distance = mob.distanceSquaredTo(*candidate);
        if (distance <= nearestDistance) {
            nearest = candidate;
            nearestDistance = distance;
        }
    }
    return nearest;
}
