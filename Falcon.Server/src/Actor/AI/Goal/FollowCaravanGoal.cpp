#include "Actor/AI/Goal/FollowCaravanGoal.h"

#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/RideSystem.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <unordered_map>
#include <utility>

namespace {
    const float SEARCH_HORIZONTAL = 9.0f;
    const float SEARCH_VERTICAL = 4.0f;
    const float MIN_JOIN_DISTANCE_SQUARED = 4.0f;
    const float MAX_FOLLOW_DISTANCE_SQUARED = 676.0f;
    const float FOLLOW_GAP = 2.0f;
    const float SPEED_BOOST = 1.2f;
    const float MAX_SPEED_MULTIPLIER = 3.0f;
    const int32_t DISTANCE_CHECK_TICKS = 40;
    const int32_t REPATH_INTERVAL = 10;

    std::unordered_map<int64_t, int64_t> &caravanHeads() {
        static std::unordered_map<int64_t, int64_t> heads;
        return heads;
    }

    bool isInCaravan(int64_t uniqueId) {
        return caravanHeads().count(uniqueId) != 0;
    }

    bool hasCaravanTail(ServerNetworkHandler &owner, int64_t uniqueId) {
        for (const auto &entry: caravanHeads()) {
            if (entry.second != uniqueId)
                continue;

            const Actor *tail = RideSystem::resolve(owner, entry.first);
            if (tail != nullptr && tail->isAlive())
                return true;
        }
        return false;
    }
}

FollowCaravanGoal::FollowCaravanGoal(float movementSpeed, float speedMultiplier, int32_t entityCount,
                                     std::shared_ptr<json::Value> filters)
        : mMovementSpeed(movementSpeed), mBaseSpeedMultiplier(speedMultiplier), mSpeedMultiplier(speedMultiplier),
          mEntityCount(entityCount), mFilters(std::move(filters)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool FollowCaravanGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getFlags().get(ActorFlag::Leashed) || isInCaravan(mob.getUniqueId()))
        return false;

    Actor *head = _findHead(owner, mob, true);
    if (head == nullptr)
        head = _findHead(owner, mob, false);
    if (head == nullptr || mob.distanceSquaredTo(*head) < MIN_JOIN_DISTANCE_SQUARED)
        return false;

    if (!head->getFlags().get(ActorFlag::Leashed) && !_isChainLeashed(owner, head->getUniqueId(), 1))
        return false;

    mHeadId = head->getUniqueId();
    caravanHeads()[mob.getUniqueId()] = mHeadId;
    return true;
}

bool FollowCaravanGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *head = RideSystem::resolve(owner, mHeadId);
    if (!isInCaravan(mob.getUniqueId()) || head == nullptr || !head->isAlive()
        || !_isChainLeashed(owner, mob.getUniqueId(), 0))
        return false;

    if (mob.distanceSquaredTo(*head) > MAX_FOLLOW_DISTANCE_SQUARED) {
        if (mSpeedMultiplier <= MAX_SPEED_MULTIPLIER) {
            mSpeedMultiplier *= SPEED_BOOST;
            mDistanceCheckTicks = DISTANCE_CHECK_TICKS;
            return true;
        }

        if (mDistanceCheckTicks == 0)
            return false;
    }

    if (mDistanceCheckTicks > 0)
        mDistanceCheckTicks--;
    return true;
}

void FollowCaravanGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mTicksUntilRepath = 0;
}

void FollowCaravanGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    caravanHeads().erase(mob.getUniqueId());
    mHeadId = 0;
    mSpeedMultiplier = mBaseSpeedMultiplier;
    mob.getNavigation().stop(mob);
}

void FollowCaravanGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *head = RideSystem::resolve(owner, mHeadId);
    if (head == nullptr)
        return;

    if (--mTicksUntilRepath > 0)
        return;

    mTicksUntilRepath = REPATH_INTERVAL;
    const Vector3f position = mob.getPosition();
    const Vector3f headPosition = head->getPosition();
    const float dx = headPosition.x - position.x;
    const float dy = headPosition.y - position.y;
    const float dz = headPosition.z - position.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance <= FOLLOW_GAP)
        return;

    const float scale = (distance - FOLLOW_GAP) / distance;
    mob.getNavigation().moveTo(Vector3f(position.x + dx * scale, position.y + dy * scale, position.z + dz * scale),
                               mMovementSpeed * mSpeedMultiplier);
}

Actor *FollowCaravanGoal::_findHead(ServerNetworkHandler &owner, const MobActor &mob, bool inCaravan) const {
    const Vector3f position = mob.getPosition();
    Actor *nearest = nullptr;
    float nearestDistance = 0.0f;

    for (auto &entry: owner.getActors()) {
        MobActor *candidate = dynamic_cast<MobActor *>(entry.second.get());
        if (candidate == nullptr || candidate == &mob || !candidate->isAlive()
            || candidate->getDimension() != mob.getDimension())
            continue;

        const Vector3f other = candidate->getPosition();
        if (std::fabs(other.x - position.x) > SEARCH_HORIZONTAL || std::fabs(other.y - position.y) > SEARCH_VERTICAL
            || std::fabs(other.z - position.z) > SEARCH_HORIZONTAL)
            continue;

        const bool linked = inCaravan ? isInCaravan(candidate->getUniqueId())
                                      : candidate->getFlags().get(ActorFlag::Leashed);
        if (!linked || hasCaravanTail(owner, candidate->getUniqueId()))
            continue;

        if (mFilters != nullptr && !EntityFilter::test(*mFilters, owner, mob, candidate))
            continue;

        const float distance = mob.distanceSquaredTo(*candidate);
        if (nearest == nullptr || distance < nearestDistance) {
            nearest = candidate;
            nearestDistance = distance;
        }
    }
    return nearest;
}

bool FollowCaravanGoal::_isChainLeashed(ServerNetworkHandler &owner, int64_t uniqueId, int32_t depth) const {
    if (depth > mEntityCount)
        return false;

    const auto found = caravanHeads().find(uniqueId);
    if (found == caravanHeads().end())
        return false;

    const Actor *head = RideSystem::resolve(owner, found->second);
    if (head == nullptr)
        return false;

    return head->getFlags().get(ActorFlag::Leashed) || _isChainLeashed(owner, found->second, depth + 1);
}
