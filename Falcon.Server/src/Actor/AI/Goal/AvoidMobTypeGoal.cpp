#include "Actor/AI/Goal/AvoidMobTypeGoal.h"

#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>
#include <utility>

namespace {
    const float FLEE_DISTANCE = 7.0f;

    float distanceSquared(const Vector3f &left, const Vector3f &right) {
        const float dx = left.x - right.x;
        const float dy = left.y - right.y;
        const float dz = left.z - right.z;
        return dx * dx + dy * dy + dz * dz;
    }

    bool isAvoidablePlayer(const ServerPlayer &player) {
        const int32_t gameType = player.getGameType();
        return player.isSpawned() && !player.isDead() && gameType != (int32_t) GameType::Creative
               && gameType != (int32_t) GameType::Spectator;
    }
}

AvoidMobTypeGoal::AvoidMobTypeGoal(std::vector<Entry> entries) : mEntries(std::move(entries)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

const Actor *AvoidMobTypeGoal::_findThreat(ServerNetworkHandler &owner, MobActor &mob, const Entry *&entry) const {
    const Actor *nearest = nullptr;
    float nearestDistance = 0.0f;

    const auto consider = [&](const Actor &candidate) {
        if (&candidate == &mob || !candidate.isAlive() || candidate.getDimension() != mob.getDimension())
            return;

        const float distance = mob.distanceSquaredTo(candidate);
        for (const Entry &option: mEntries) {
            if (distance > option.mMaxDistance * option.mMaxDistance)
                continue;
            if (option.mFilters != nullptr && !EntityFilter::test(*option.mFilters, owner, mob, &candidate))
                continue;
            if (nearest == nullptr || distance < nearestDistance) {
                nearest = &candidate;
                nearestDistance = distance;
                entry = &option;
            }
        }
    };

    for (auto &player: owner.getPlayers()) {
        if (isAvoidablePlayer(player.second))
            consider(player.second);
    }
    for (auto &actor: owner.getActors())
        consider(*actor.second);

    return nearest;
}

bool AvoidMobTypeGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Entry *entry = nullptr;
    const Actor *threat = _findThreat(owner, mob, entry);
    if (threat == nullptr)
        return false;

    const Vector3f position = mob.getPosition();
    mThreatPosition = threat->getPosition();
    float awayX = position.x - mThreatPosition.x;
    float awayZ = position.z - mThreatPosition.z;
    const float length = std::sqrt(awayX * awayX + awayZ * awayZ);
    if (length < 0.001f) {
        awayX = 1.0f;
        awayZ = 0.0f;
    } else {
        awayX /= length;
        awayZ /= length;
    }

    mDestination = Vector3f(position.x + awayX * FLEE_DISTANCE, position.y, position.z + awayZ * FLEE_DISTANCE);
    mEntry = entry;
    return true;
}

bool AvoidMobTypeGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mEntry != nullptr && !mob.getNavigation().isDone();
}

void AvoidMobTypeGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mSprinting = false;
    mob.getNavigation().moveTo(mDestination, mEntry->mWalkSpeed);
}

void AvoidMobTypeGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mEntry = nullptr;
    mob.getNavigation().stop(mob);
}

void AvoidMobTypeGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (mEntry == nullptr)
        return;

    const bool sprint = distanceSquared(mob.getPosition(), mThreatPosition)
                        < mEntry->mSprintDistance * mEntry->mSprintDistance;
    if (sprint == mSprinting)
        return;

    mSprinting = sprint;
    mob.getNavigation().moveTo(mDestination, sprint ? mEntry->mSprintSpeed : mEntry->mWalkSpeed);
}
