#include "Actor/AI/Goal/NearestAttackableTargetGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

NearestAttackableTargetGoal::NearestAttackableTargetGoal(float range) : mRangeSquared(range * range) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Target);
}

bool NearestAttackableTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return _findNearest(owner, mob) != nullptr;
}

bool NearestAttackableTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *target = mob.getTarget(owner);
    return target != nullptr && _inRange(mob, *target);
}

void NearestAttackableTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *nearest = _findNearest(owner, mob);
    if (nearest != nullptr)
        mob.setTarget(nearest->getRuntimeId());
}

void NearestAttackableTargetGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.clearTarget();
}

ServerPlayer *NearestAttackableTargetGoal::_findNearest(ServerNetworkHandler &owner, const MobActor &mob) const {
    ServerPlayer *nearest = nullptr;
    float nearestDistance = mRangeSquared;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!mob.canTarget(player))
            continue;

        const float distance = mob.distanceSquaredTo(player);
        if (distance > nearestDistance)
            continue;

        nearest = &player;
        nearestDistance = distance;
    }
    return nearest;
}

bool NearestAttackableTargetGoal::_inRange(const MobActor &mob, const ServerPlayer &player) const {
    return mob.distanceSquaredTo(player) <= mRangeSquared;
}
