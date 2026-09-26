#include "Actor/AI/Goal/MoveToBlockGoal.h"

#include "Actor/Definition/EntityEvents.h"
#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/Movement/ActorPushSystem.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace {
    const int32_t SEARCH_COOLDOWN_TICKS = 20;

    std::mt19937 &moveToBlockRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

MoveToBlockGoal::MoveToBlockGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool MoveToBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mSearchCooldown > 0) {
        --mSearchCooldown;
        return false;
    }

    if (mSettings.mTickInterval > 1
        && std::uniform_int_distribution<int32_t>(0, mSettings.mTickInterval - 1)(moveToBlockRandom()) != 0)
        return false;

    if (mSettings.mStartChance < 1.0f
        && std::uniform_real_distribution<float>(0.0f, 1.0f)(moveToBlockRandom()) >= mSettings.mStartChance)
        return false;

    if (_findTarget(owner, mob))
        return true;

    mSearchCooldown = SEARCH_COOLDOWN_TICKS;
    return false;
}

bool MoveToBlockGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mFinished)
        return false;

    const BlockState *state = owner.getLevelFor(mob).peekBlockPtr(mTarget.x, mTarget.y, mTarget.z);
    if (state == nullptr || !_isTargetBlock(owner, mob, mTarget, *state))
        return false;

    return mReached || _isReached(mob) || !mob.getNavigation().isDone();
}

void MoveToBlockGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mReached = false;
    mFinished = false;
    mStayTicks = 0;
    mob.getNavigation().moveTo(_targetPoint(), mSettings.mSpeed);
}

void MoveToBlockGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mob.getLookControl().clear();
    mReached = false;
    mFinished = false;
    mStayTicks = 0;
}

void MoveToBlockGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    mob.getLookControl().setLookAt(_targetPoint());

    if (!mReached) {
        if (!_isReached(mob))
            return;

        mReached = true;
        mob.getNavigation().stop(mob);
        _fire(owner, mob, mSettings.mOnReach.get());
    }

    if (mStayTicks++ < mSettings.mStayTicks)
        return;

    _fire(owner, mob, mSettings.mOnStayCompleted.get());
    mFinished = true;
}

bool MoveToBlockGoal::_findTarget(ServerNetworkHandler &owner, MobActor &mob) {
    Level &level = owner.getLevelFor(mob);
    const Vector3f position = mob.getPosition();
    const int32_t originX = (int32_t) std::floor(position.x);
    const int32_t originY = (int32_t) std::floor(position.y);
    const int32_t originZ = (int32_t) std::floor(position.z);
    const int32_t minY = std::max(originY - mSettings.mSearchHeight, LevelChunk::MIN_Y);
    const int32_t maxY = std::min(originY + mSettings.mSearchHeight, LevelChunk::MAX_Y);

    bool found = false;
    float bestDistance = 0.0f;
    int32_t candidates = 0;

    for (int32_t x = originX - mSettings.mSearchRange; x <= originX + mSettings.mSearchRange; ++x) {
        for (int32_t z = originZ - mSettings.mSearchRange; z <= originZ + mSettings.mSearchRange; ++z) {
            LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
            if (chunk == nullptr)
                continue;

            for (int32_t y = minY; y <= maxY; ++y) {
                const BlockState &state = chunk->getBlock(x & 15, y, z & 15);
                const Vector3i candidate(x, y, z);
                if (!_isTargetBlock(owner, mob, candidate, state))
                    continue;

                if (mSettings.mRandomTarget) {
                    ++candidates;
                    if (std::uniform_int_distribution<int32_t>(0, candidates - 1)(moveToBlockRandom()) == 0) {
                        mTarget = candidate;
                        found = true;
                    }
                    continue;
                }

                const float dx = (float) x + 0.5f - position.x;
                const float dy = (float) y + 0.5f - position.y;
                const float dz = (float) z + 0.5f - position.z;
                const float distance = dx * dx + dy * dy + dz * dz;
                if (!found || distance < bestDistance) {
                    bestDistance = distance;
                    mTarget = candidate;
                    found = true;
                }
            }
        }
    }

    return found;
}

bool MoveToBlockGoal::_isTargetBlock(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position,
                                     const BlockState &state) const {
    if (mSettings.mTargetBlocks.count(state.mName) == 0)
        return false;

    if (mSettings.mTargetFilters == nullptr)
        return true;

    mob.setEventBlock(position);
    const bool result = EntityFilter::test(*mSettings.mTargetFilters, owner, mob);
    mob.clearEventBlock();
    return result;
}

Vector3f MoveToBlockGoal::_targetPoint() const {
    return Vector3f((float) mTarget.x + 0.5f + mSettings.mTargetOffset.x,
                    (float) mTarget.y + 0.5f + mSettings.mTargetOffset.y,
                    (float) mTarget.z + 0.5f + mSettings.mTargetOffset.z);
}

bool MoveToBlockGoal::_isReached(const MobActor &mob) const {
    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(mob);
    const Vector3f point = _targetPoint();
    const float dx = std::max({box.mMinX - point.x, 0.0f, point.x - box.mMaxX});
    const float dy = std::max({box.mMinY - point.y, 0.0f, point.y - box.mMaxY});
    const float dz = std::max({box.mMinZ - point.z, 0.0f, point.z - box.mMaxZ});
    return dx * dx + dy * dy + dz * dz <= mSettings.mGoalRadius * mSettings.mGoalRadius;
}

void MoveToBlockGoal::_fire(ServerNetworkHandler &owner, MobActor &mob, const json::Value *triggers) const {
    if (triggers == nullptr)
        return;

    mob.setEventBlock(mTarget);
    EntityEvents::fireTriggers(owner, mob, triggers);
    mob.clearEventBlock();
}
