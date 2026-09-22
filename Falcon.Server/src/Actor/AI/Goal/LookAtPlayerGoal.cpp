#include "Actor/AI/Goal/LookAtPlayerGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <random>

namespace {
    std::mt19937 &lookRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

LookAtPlayerGoal::LookAtPlayerGoal(float range, int32_t probability, int32_t total, int32_t duration,
                                   int32_t checkInterval)
        : mRange(range), mProbability(probability), mTotal(total), mDuration(duration),
          mCheckInterval(checkInterval) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Look);
}

bool LookAtPlayerGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mTicksUntilCheck > 0) {
        mTicksUntilCheck--;
        return false;
    }

    mTicksUntilCheck = mCheckInterval;
    if (std::uniform_int_distribution<int32_t>(0, mTotal - 1)(lookRandom()) >= mProbability)
        return false;

    return _findNearestPlayer(owner, mob) != nullptr;
}

bool LookAtPlayerGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mTicksRunning <= mDuration && _findNearestPlayer(owner, mob) != nullptr;
}

void LookAtPlayerGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mTicksRunning = 0;
    mob.getLookControl().setPitchEnabled(true);
}

void LookAtPlayerGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mTicksRunning = 0;
    mob.getLookControl().clear();
    mob.getLookControl().setPitchEnabled(false);
}

void LookAtPlayerGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    mTicksRunning++;

    const ServerPlayer *player = _findNearestPlayer(owner, mob);
    if (player != nullptr)
        mob.getLookControl().setLookAt(player->getPosition());
}

ServerPlayer *LookAtPlayerGoal::_findNearestPlayer(ServerNetworkHandler &owner, const MobActor &mob) const {
    const float rangeSquared = mRange * mRange;

    ServerPlayer *nearest = nullptr;
    float nearestDistance = rangeSquared;
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.getDimension() != mob.getDimension())
            continue;

        const float distance = mob.distanceSquaredTo(player);
        if (distance > nearestDistance)
            continue;

        nearest = &player;
        nearestDistance = distance;
    }

    return nearest;
}
