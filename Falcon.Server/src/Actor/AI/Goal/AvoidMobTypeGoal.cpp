#include "Actor/AI/Goal/AvoidMobTypeGoal.h"

#include "Actor/Definition/EntityEvents.h"
#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace {
    const float FLEE_DISTANCE = 7.0f;
    const float DEFAULT_GROUP_RADIUS = 16.0f;

    std::mt19937 &avoidMobRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

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

AvoidMobTypeGoal::AvoidMobTypeGoal(std::vector<Entry> entries, Options options)
        : mEntries(std::move(entries)), mOptions(std::move(options)) {
    mOptions.mMinSoundInterval = std::max(1, mOptions.mMinSoundInterval);
    mOptions.mMaxSoundInterval = std::max(mOptions.mMinSoundInterval, mOptions.mMaxSoundInterval);
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

int32_t AvoidMobTypeGoal::_nextSound() const {
    return std::uniform_int_distribution<int32_t>(mOptions.mMinSoundInterval, mOptions.mMaxSoundInterval)(
            avoidMobRandom());
}

bool AvoidMobTypeGoal::_isOutnumbered(ServerNetworkHandler &owner, MobActor &mob, const Entry &entry) const {
    int32_t threats = 0;
    for (auto &actor: owner.getActors()) {
        const Actor &candidate = *actor.second;
        if (&candidate == &mob || !candidate.isAlive() || candidate.getDimension() != mob.getDimension()
            || mob.distanceSquaredTo(candidate) > entry.mMaxDistance * entry.mMaxDistance)
            continue;
        if (entry.mFilters == nullptr || EntityFilter::test(*entry.mFilters, owner, mob, &candidate))
            ++threats;
    }

    const json::Value *groupSize = mob.getComponent("minecraft:group_size");
    const json::Value *radiusValue = groupSize == nullptr ? nullptr : groupSize->get("radius");
    const float radius = radiusValue == nullptr ? DEFAULT_GROUP_RADIUS : (float) radiusValue->number(DEFAULT_GROUP_RADIUS);
    const json::Value *groupFilters = groupSize == nullptr ? nullptr : groupSize->get("filters");

    int32_t allies = 0;
    for (auto &actor: owner.getActors()) {
        const MobActor *ally = dynamic_cast<const MobActor *>(actor.second.get());
        if (ally == nullptr || !ally->isAlive() || ally->getDimension() != mob.getDimension()
            || mob.distanceSquaredTo(*ally) > radius * radius)
            continue;
        if (groupFilters == nullptr || EntityFilter::test(*groupFilters, owner, *ally))
            ++allies;
    }
    return threats > allies;
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
            if (option.mCheckIfOutnumbered && !_isOutnumbered(owner, mob, option))
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
    mSoundTicks = 0;
    if (mOptions.mRemoveTarget)
        mob.clearTarget();
    mob.getNavigation().moveTo(mDestination, mEntry->mWalkSpeed);
}

void AvoidMobTypeGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    const bool escaped = mEntry != nullptr;
    mEntry = nullptr;
    mob.getNavigation().stop(mob);
    if (escaped)
        EntityEvents::fireTrigger(owner, mob, mOptions.mOnEscape.get());
}

void AvoidMobTypeGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mEntry == nullptr)
        return;

    if (!mOptions.mSound.empty() && --mSoundTicks <= 0) {
        mSoundTicks = _nextSound();
        mob.playDefinitionSound(owner, mOptions.mSound);
    }

    const bool sprint = distanceSquared(mob.getPosition(), mThreatPosition)
                        < mEntry->mSprintDistance * mEntry->mSprintDistance;
    if (sprint == mSprinting)
        return;

    mSprinting = sprint;
    mob.getNavigation().moveTo(mDestination, sprint ? mEntry->mSprintSpeed : mEntry->mWalkSpeed);
}
