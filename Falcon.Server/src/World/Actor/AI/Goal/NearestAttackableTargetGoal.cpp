#include "World/Actor/AI/Goal/NearestAttackableTargetGoal.h"

#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace {
    std::mt19937 &targetScanRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

NearestAttackableTargetGoal::NearestAttackableTargetGoal(float range, std::vector<Entry> entries, int32_t scanInterval)
        : mRangeSquared(range * range), mScanInterval(scanInterval), mMaxRangeSquared(range * range),
          mEntries(std::move(entries)) {
    if (!mEntries.empty()) {
        mMaxRangeSquared = 0.0f;
        for (const Entry &entry: mEntries) {
            const float limit = entry.mMaxDistance > 0.0f ? entry.mMaxDistance * entry.mMaxDistance : mRangeSquared;
            mMaxRangeSquared = std::max(mMaxRangeSquared, limit);
        }
    }
    setRequiredControlFlags((uint8_t) GoalControlFlag::Target);
}

bool NearestAttackableTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mScanInterval > 0
        && std::uniform_int_distribution<int32_t>(0, mScanInterval - 1)(targetScanRandom()) != 0)
        return false;

    const Actor *nearest = _findNearest(owner, mob);
    mFoundTargetRuntimeId = nearest == nullptr ? 0 : nearest->getRuntimeId();
    return nearest != nullptr;
}

bool NearestAttackableTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return target != nullptr && mob.distanceSquaredTo(*target) <= mRangeSquared;
}

void NearestAttackableTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    if (mFoundTargetRuntimeId != 0)
        mob.setTarget(owner, mFoundTargetRuntimeId);
    mFoundTargetRuntimeId = 0;
}

void NearestAttackableTargetGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.clearTarget();
}

bool NearestAttackableTargetGoal::_matches(ServerNetworkHandler &owner, MobActor &mob, const Actor &candidate) const {
    const float distance = mob.distanceSquaredTo(candidate);
    if (distance > mMaxRangeSquared || !mob.canTarget(candidate))
        return false;

    if (mEntries.empty())
        return candidate.isPlayer() && distance <= mRangeSquared;

    for (const Entry &entry: mEntries) {
        const float limit = entry.mMaxDistance > 0.0f ? entry.mMaxDistance * entry.mMaxDistance : mRangeSquared;
        if (distance > limit)
            continue;
        if (entry.mFilters == nullptr || EntityFilter::test(*entry.mFilters, owner, mob, &candidate))
            return true;
    }
    return false;
}

Actor *NearestAttackableTargetGoal::_findNearest(ServerNetworkHandler &owner, MobActor &mob) const {
    Actor *nearest = nullptr;
    float nearestDistance = 0.0f;

    const auto consider = [&](Actor &candidate) {
        if (!_matches(owner, mob, candidate))
            return;

        const float distance = mob.distanceSquaredTo(candidate);
        if (nearest == nullptr || distance < nearestDistance) {
            nearest = &candidate;
            nearestDistance = distance;
        }
    };

    for (auto &entry: owner.getPlayers())
        consider(entry.second);
    if (!mEntries.empty())
        owner.forEachActorNear(mob.getPosition(), std::sqrt(mMaxRangeSquared), consider);

    return nearest;
}
