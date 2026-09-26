#include "Actor/AI/Goal/CircleAroundAnchorGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/BlockShape.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    const float DEGREES_TO_RADIANS = 0.017453292f;
    const float TWO_PI = 6.2831855f;
    const float MIN_ESCAPE_OFFSET = 1.0f;

    std::mt19937 &circleRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float randomBetween(float minimum, float maximum) {
        if (maximum <= minimum)
            return minimum;
        return std::uniform_real_distribution<float>(minimum, maximum)(circleRandom());
    }

    bool roll(float chance) {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(circleRandom()) < chance;
    }
}

CircleAroundAnchorGoal::CircleAroundAnchorGoal(const Settings &settings) : mSettings(settings) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool CircleAroundAnchorGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return true;
}

void CircleAroundAnchorGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mRadius = randomBetween(mSettings.mMinRadius, mSettings.mMaxRadius);
    mHeightOffset = randomBetween(mSettings.mMinHeightOffset, mSettings.mMaxHeightOffset);
    mAngle = randomBetween(0.0f, TWO_PI);
    mClockwise = roll(0.5f);
    mAnchorTargetId = 0;
    _updateAnchor(owner, mob);
    _selectNext();
    mob.getLookControl().setPitchEnabled(true);
}

void CircleAroundAnchorGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getMoveControl().stop();
    mob.getLookControl().clear();
    mob.getLookControl().setPitchEnabled(false);
}

void CircleAroundAnchorGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const uint64_t previousTarget = mAnchorTargetId;
    _updateAnchor(owner, mob);
    if (mAnchorTargetId != previousTarget)
        _selectNext();

    if (roll(mSettings.mHeightAdjustmentChance))
        mHeightOffset = randomBetween(mSettings.mMinHeightOffset, mSettings.mMaxHeightOffset);

    if (roll(mSettings.mRadiusAdjustmentChance)) {
        mRadius += mSettings.mRadiusChange;
        if (mRadius > mSettings.mMaxRadius) {
            mRadius = mSettings.mMinRadius;
            mClockwise = !mClockwise;
        }
    }

    const Vector3f position = mob.getPosition();
    const float dx = mMoveTarget.x - position.x;
    const float dy = mMoveTarget.y - position.y;
    const float dz = mMoveTarget.z - position.z;
    if (dx * dx + dy * dy + dz * dz < mSettings.mGoalRadius * mSettings.mGoalRadius)
        _selectNext();

    Level &level = owner.getLevelFor(mob);
    if (mMoveTarget.y < position.y && _isBlocked(level, position, -1)) {
        mHeightOffset = std::max(MIN_ESCAPE_OFFSET, mHeightOffset);
        _selectNext();
    }

    if (mMoveTarget.y > position.y && _isBlocked(level, position, 1)) {
        mHeightOffset = std::min(-MIN_ESCAPE_OFFSET, mHeightOffset);
        _selectNext();
    }

    mob.getMoveControl().setWantedPosition(mMoveTarget, mSettings.mSpeed);
    mob.getLookControl().setLookAt(mMoveTarget);
}

void CircleAroundAnchorGoal::_updateAnchor(ServerNetworkHandler &owner, const MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr) {
        mAnchorTargetId = 0;
        if (!mHasAnchor) {
            mAnchor = mob.getPosition();
            mHasAnchor = true;
        }
        return;
    }

    if (target->getRuntimeId() == mAnchorTargetId)
        return;

    const Vector3f position = target->getPosition();
    mAnchor = Vector3f(position.x,
                       position.y + randomBetween(mSettings.mMinHeightAboveTarget, mSettings.mMaxHeightAboveTarget),
                       position.z);
    mAnchorTargetId = target->getRuntimeId();
    mHasAnchor = true;
}

void CircleAroundAnchorGoal::_selectNext() {
    mAngle += (mClockwise ? 1.0f : -1.0f) * mSettings.mAngleChange * DEGREES_TO_RADIANS;
    mMoveTarget = Vector3f(mAnchor.x + mRadius * std::cos(mAngle), mAnchor.y + mHeightOffset,
                           mAnchor.z + mRadius * std::sin(mAngle));
}

bool CircleAroundAnchorGoal::_isBlocked(Level &level, const Vector3f &position, int32_t offsetY) {
    const BlockState *state = level.peekBlockPtr((int32_t) std::floor(position.x),
                                                 (int32_t) std::floor(position.y) + offsetY,
                                                 (int32_t) std::floor(position.z));
    return state == nullptr || BlockShape::hasCollision(*state);
}
