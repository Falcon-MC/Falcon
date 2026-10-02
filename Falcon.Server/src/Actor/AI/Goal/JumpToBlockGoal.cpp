#include "Actor/AI/Goal/JumpToBlockGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/BlockShape.h"
#include "Block/Blocks/LiquidView.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <utility>

namespace {
    const std::array<float, 4> JUMP_ANGLES = {65.0f, 70.0f, 75.0f, 80.0f};
    const float DEGREES_TO_RADIANS = 0.017453292f;
    const int32_t CANDIDATE_ATTEMPTS = 20;
    const int32_t RETRY_TICKS = 20;
    const int32_t MAX_TRAJECTORY_STEPS = 60;
    const float MIN_HORIZONTAL = 1.0e-4f;

    std::mt19937 &jumpRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t randomOffset(int32_t range) {
        return range <= 0 ? 0 : std::uniform_int_distribution<int32_t>(-range, range)(jumpRandom());
    }

    bool hasCollisionAt(Level &level, float x, float y, float z) {
        const BlockState *state = level.peekBlockPtr((int32_t) std::floor(x), (int32_t) std::floor(y),
                                                     (int32_t) std::floor(z));
        return state == nullptr || BlockShape::hasCollision(*state);
    }
}

JumpToBlockGoal::JumpToBlockGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Jump);
}

bool JumpToBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (owner.getCurrentTick() < mNextJumpTick || !mob.isOnGround())
        return false;

    Level &level = owner.getLevelFor(mob);
    const bool preferred = !mSettings.mPreferredBlocks.empty()
                           && std::uniform_real_distribution<float>(0.0f, 1.0f)(jumpRandom())
                              < mSettings.mPreferredBlocksChance;
    if ((preferred && _findTarget(level, mob, true)) || _findTarget(level, mob, false))
        return true;

    mNextJumpTick = owner.getCurrentTick() + RETRY_TICKS;
    return false;
}

bool JumpToBlockGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return !mob.isOnGround();
}

void JumpToBlockGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mob.getLookControl().setLookAt(mTarget);
    mob.setMotion(mMotion);
    mob.setOnGround(false);
}

void JumpToBlockGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    const int32_t maximum = std::max(mSettings.mMinCooldownTicks, mSettings.mMaxCooldownTicks);
    mNextJumpTick = owner.getCurrentTick()
                    + std::uniform_int_distribution<int32_t>(mSettings.mMinCooldownTicks, maximum)(jumpRandom());
    mob.getLookControl().clear();
}

bool JumpToBlockGoal::_findTarget(Level &level, const MobActor &mob, bool preferredOnly) {
    const Vector3f position = mob.getPosition();
    const int32_t originX = (int32_t) std::floor(position.x);
    const int32_t originY = (int32_t) std::floor(position.y);
    const int32_t originZ = (int32_t) std::floor(position.z);

    for (int32_t attempt = 0; attempt < CANDIDATE_ATTEMPTS; ++attempt) {
        const int32_t x = originX + randomOffset(mSettings.mSearchWidth);
        const int32_t y = originY + randomOffset(mSettings.mSearchHeight);
        const int32_t z = originZ + randomOffset(mSettings.mSearchWidth);
        if (!_isLandingSpot(level, mob, x, y, z, preferredOnly))
            continue;

        const Vector3f target((float) x + 0.5f, (float) y, (float) z + 0.5f);
        Vector3f motion;
        if (!_computeMotion(level, mob, target, motion))
            continue;

        mTarget = target;
        mMotion = motion;
        return true;
    }
    return false;
}

bool JumpToBlockGoal::_isLandingSpot(Level &level, const MobActor &mob, int32_t x, int32_t y, int32_t z,
                                     bool preferredOnly) const {
    const Vector3f position = mob.getPosition();
    if (x == (int32_t) std::floor(position.x) && z == (int32_t) std::floor(position.z))
        return false;

    const float dx = (float) x + 0.5f - position.x;
    const float dz = (float) z + 0.5f - position.z;
    if (dx * dx + dz * dz < mSettings.mMinimumDistance * mSettings.mMinimumDistance)
        return false;

    const BlockState *ground = level.peekBlockPtr(x, y - 1, z);
    if (ground == nullptr || !BlockShape::hasCollision(*ground) || mSettings.mForbiddenBlocks.count(ground->mName) != 0)
        return false;

    if (preferredOnly && mSettings.mPreferredBlocks.count(ground->mName) == 0)
        return false;

    const int32_t clearance = std::max(1, (int32_t) std::ceil(mob.getSize().mHeight));
    for (int32_t offset = 0; offset < clearance; ++offset) {
        const BlockState *space = level.peekBlockPtr(x, y + offset, z);
        if (space == nullptr || BlockShape::hasCollision(*space) || LiquidView(*space).isLiquid()
            || mSettings.mForbiddenBlocks.count(space->mName) != 0)
            return false;
    }
    return true;
}

bool JumpToBlockGoal::_computeMotion(Level &level, const MobActor &mob, const Vector3f &target,
                                     Vector3f &motion) const {
    const Vector3f position = mob.getPosition();
    const float dx = target.x - position.x;
    const float dy = target.y - position.y;
    const float dz = target.z - position.z;
    const float horizontal = std::sqrt(dx * dx + dz * dz);
    if (horizontal <= MIN_HORIZONTAL)
        return false;

    const float gravity = mob.getPhysics().mGravity;
    std::array<float, 4> angles = JUMP_ANGLES;
    std::shuffle(angles.begin(), angles.end(), jumpRandom());

    for (const float angle: angles) {
        const float radians = angle * DEGREES_TO_RADIANS;
        const float cosine = std::cos(radians);
        const float lift = horizontal * std::tan(radians) - dy;
        if (lift <= 0.0f)
            continue;

        const float speed = std::sqrt(gravity * horizontal * horizontal / (2.0f * cosine * cosine * lift));
        if (speed > mSettings.mMaxVelocity)
            continue;

        const float horizontalSpeed = speed * cosine;
        const Vector3f candidate(dx / horizontal * horizontalSpeed, speed * std::sin(radians),
                                 dz / horizontal * horizontalSpeed);
        if (!_isTrajectoryClear(level, mob, candidate, horizontal))
            continue;

        motion = candidate;
        return true;
    }
    return false;
}

bool JumpToBlockGoal::_isTrajectoryClear(Level &level, const MobActor &mob, Vector3f motion,
                                         float horizontalDistance) const {
    const float horizontalSpeed = std::sqrt(motion.x * motion.x + motion.z * motion.z);
    if (horizontalSpeed <= MIN_HORIZONTAL)
        return false;

    const float gravity = mob.getPhysics().mGravity;
    const float height = mob.getSize().mHeight * mSettings.mScaleFactor;
    const int32_t steps = std::min(MAX_TRAJECTORY_STEPS, (int32_t) std::ceil(horizontalDistance / horizontalSpeed));
    Vector3f point = mob.getPosition();

    for (int32_t step = 0; step < steps - 1; ++step) {
        point = Vector3f(point.x + motion.x, point.y + motion.y, point.z + motion.z);
        motion.y -= gravity;
        if (hasCollisionAt(level, point.x, point.y, point.z)
            || hasCollisionAt(level, point.x, point.y + height, point.z))
            return false;
    }
    return true;
}
