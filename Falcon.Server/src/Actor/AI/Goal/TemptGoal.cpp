#include "Actor/AI/Goal/TemptGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <utility>

namespace {
    const float STOP_DISTANCE_SQUARED = 6.25f;
    const int32_t REPATH_INTERVAL = 10;
}

TemptGoal::TemptGoal(float speed, float range, BehaviorItems items)
        : mSpeed(speed), mRange(range), mItems(std::move(items)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool TemptGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return !mItems.isEmpty() && _findTempter(owner, mob) != nullptr;
}

void TemptGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mTicksUntilRepath = 0;
    mob.getLookControl().setPitchEnabled(true);
}

void TemptGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mob.getLookControl().clear();
    mob.getLookControl().setPitchEnabled(false);
}

void TemptGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *tempter = _findTempter(owner, mob);
    if (tempter == nullptr)
        return;

    mob.getLookControl().setLookAt(tempter->getPosition());
    if (mob.distanceSquaredTo(*tempter) <= STOP_DISTANCE_SQUARED) {
        mob.getNavigation().stop(mob);
        return;
    }

    if (--mTicksUntilRepath > 0)
        return;

    mTicksUntilRepath = REPATH_INTERVAL;
    mob.getNavigation().moveTo(tempter->getPosition(), mSpeed);
}

ServerPlayer *TemptGoal::_findTempter(ServerNetworkHandler &owner, const MobActor &mob) const {
    ServerPlayer *nearest = nullptr;
    float nearestDistance = mRange * mRange;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.getDimension() != mob.getDimension()
            || player.getGameType() == (int32_t) GameType::Spectator)
            continue;

        const float distance = mob.distanceSquaredTo(player);
        if (distance > nearestDistance || !mItems.contains(player.getInventory().getItemInHand()))
            continue;

        nearest = &player;
        nearestDistance = distance;
    }
    return nearest;
}
