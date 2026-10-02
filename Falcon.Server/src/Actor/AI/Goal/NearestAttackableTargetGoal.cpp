#include "Actor/AI/Goal/NearestAttackableTargetGoal.h"

#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <utility>

NearestAttackableTargetGoal::NearestAttackableTargetGoal(float range, std::vector<Entry> entries)
        : mRangeSquared(range * range), mEntries(std::move(entries)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Target);
}

bool NearestAttackableTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return _findNearest(owner, mob) != nullptr;
}

bool NearestAttackableTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return target != nullptr && mob.distanceSquaredTo(*target) <= mRangeSquared;
}

void NearestAttackableTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *nearest = _findNearest(owner, mob);
    if (nearest != nullptr)
        mob.setTarget(owner, nearest->getRuntimeId());
}

void NearestAttackableTargetGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.clearTarget();
}

bool NearestAttackableTargetGoal::_matches(ServerNetworkHandler &owner, MobActor &mob, const Actor &candidate) const {
    if (!mob.canTarget(candidate))
        return false;

    const float distance = mob.distanceSquaredTo(candidate);
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
    if (!mEntries.empty()) {
        for (auto &entry: owner.getActors())
            consider(*entry.second);
    }

    return nearest;
}
