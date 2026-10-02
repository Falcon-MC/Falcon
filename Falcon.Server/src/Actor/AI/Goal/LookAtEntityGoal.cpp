#include "Actor/AI/Goal/LookAtEntityGoal.h"

#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Core/Math/MathConstants.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace {
    const float FULL_TURN = 360.0f;

    std::mt19937 &lookEntityRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

LookAtEntityGoal::LookAtEntityGoal(Settings settings) : mSettings(std::move(settings)) {
    mSettings.mMaxLookTicks = std::max(mSettings.mMinLookTicks, mSettings.mMaxLookTicks);
    setRequiredControlFlags((uint8_t) GoalControlFlag::Look);
}

bool LookAtEntityGoal::_accepts(ServerNetworkHandler &owner, const MobActor &mob, const Actor &candidate) const {
    if (&candidate == &mob || !candidate.isAlive() || candidate.getDimension() != mob.getDimension())
        return false;
    if (mob.distanceSquaredTo(candidate) > mSettings.mRange * mSettings.mRange)
        return false;
    if (mSettings.mFilters != nullptr && !EntityFilter::test(*mSettings.mFilters, owner, mob, &candidate))
        return false;
    if (mSettings.mHorizontalAngle >= FULL_TURN)
        return true;

    const Vector3f position = mob.getPosition();
    const Vector3f other = candidate.getPosition();
    const float towards = std::atan2(-(other.x - position.x), other.z - position.z) / MathConstants::DEGREES_TO_RADIANS_F;
    const float difference = std::fabs(std::remainder(towards - mob.getRotation().y, FULL_TURN));
    return difference <= mSettings.mHorizontalAngle * 0.5f;
}

Actor *LookAtEntityGoal::_findNearest(ServerNetworkHandler &owner, const MobActor &mob) const {
    Actor *nearest = nullptr;
    float nearestDistance = 0.0f;
    const auto consider = [&](Actor &candidate) {
        if (!_accepts(owner, mob, candidate))
            return;

        const float distance = mob.distanceSquaredTo(candidate);
        if (nearest == nullptr || distance < nearestDistance) {
            nearest = &candidate;
            nearestDistance = distance;
        }
    };

    for (auto &entry: owner.getPlayers()) {
        if (entry.second.isSpawned() && !entry.second.isDead())
            consider(entry.second);
    }
    for (auto &entry: owner.getActors())
        consider(*entry.second);
    return nearest;
}

Actor *LookAtEntityGoal::_current(ServerNetworkHandler &owner) const {
    return mTargetRuntimeId == 0 ? nullptr : MobActor::findActor(owner, mTargetRuntimeId);
}

bool LookAtEntityGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(lookEntityRandom()) >= mSettings.mProbability)
        return false;

    const Actor *target = _findNearest(owner, mob);
    if (target == nullptr)
        return false;

    mTargetRuntimeId = target->getRuntimeId();
    return true;
}

bool LookAtEntityGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = _current(owner);
    return mRemainingTicks > 0 && target != nullptr && _accepts(owner, mob, *target);
}

void LookAtEntityGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mRemainingTicks = std::uniform_int_distribution<int32_t>(mSettings.mMinLookTicks,
                                                            mSettings.mMaxLookTicks)(lookEntityRandom());
    mob.getLookControl().setPitchEnabled(true);
}

void LookAtEntityGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mTargetRuntimeId = 0;
    mob.getLookControl().clear();
    mob.getLookControl().setPitchEnabled(false);
}

void LookAtEntityGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    --mRemainingTicks;
    const Actor *target = _current(owner);
    if (target != nullptr)
        mob.getLookControl().setLookAt(target->getPosition());
}
