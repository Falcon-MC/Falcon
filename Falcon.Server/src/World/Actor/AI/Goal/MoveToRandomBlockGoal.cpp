#include "World/Actor/AI/Goal/MoveToRandomBlockGoal.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Goal/RandomGroundPosition.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Level.h"

#include <cmath>

namespace {
    const int32_t SEARCH_HEIGHT = 8;
    const int32_t STUCK_LIMIT = 4;
    const int32_t STUCK_COOLDOWN_TICKS = 400;
    const int32_t SIDESTEP_AFTER_TICKS = 2;
    const float SIDESTEP_DISTANCE = 10.0f;
    const float MOVED_DISTANCE = 1.0f;

    // the game's facings: down, up, north, south, west, east
    enum Facing : uint8_t { Down, Up, North, South, West, East };

    Facing dominantFacing(const Vector3f &delta) {
        const float x = std::fabs(delta.x);
        const float y = std::fabs(delta.y);
        const float z = std::fabs(delta.z);
        if (y > x && y > z)
            return delta.y > 0.0f ? Up : Down;
        if (x > y && x > z)
            return delta.x > 0.0f ? East : West;
        return delta.z > 0.0f ? South : North;
    }

    // stuck: a quarter turn, either way at random; up and down swap
    Facing sidestepFacing(Facing facing, std::mt19937 &random) {
        if (facing == Up || facing == Down)
            return facing == Up ? Down : Up;

        const bool clockwise = std::uniform_int_distribution<int>(0, 1)(random) == 1;
        switch (facing) {
            case North:
                return clockwise ? East : West;
            case South:
                return clockwise ? West : East;
            case West:
                return clockwise ? North : South;
            default:
                return clockwise ? South : North;
        }
    }

    Vector3f facingOffset(Facing facing, float distance) {
        switch (facing) {
            case Down:
                return Vector3f(0.0f, -distance, 0.0f);
            case Up:
                return Vector3f(0.0f, distance, 0.0f);
            case North:
                return Vector3f(0.0f, 0.0f, -distance);
            case South:
                return Vector3f(0.0f, 0.0f, distance);
            case West:
                return Vector3f(-distance, 0.0f, 0.0f);
            default:
                return Vector3f(distance, 0.0f, 0.0f);
        }
    }
}

MoveToRandomBlockGoal::MoveToRandomBlockGoal(float speed, float withinRadius, int32_t blockDistance)
        : mSpeed(speed), mWithinRadius(withinRadius), mBlockDistance(blockDistance), mRandom(std::random_device{}()) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool MoveToRandomBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mCooldown > 0) {
        --mCooldown;
        return false;
    }

    Level &level = owner.getLevelFor(mob);
    if (!mHasTarget) {
        Vector3i picked;
        if (!RandomGroundPosition::find(level, mob, mBlockDistance, SEARCH_HEIGHT, nullptr, mRandom, picked))
            return false;

        // the game drops the pick on the top solid block of its column
        const int32_t top = level.getAboveTopSolidBlock(picked.x, picked.y, picked.z);
        mTarget = Vector3f((float) picked.x, (float) top, (float) picked.z);
        mHasTarget = true;
    }

    if (_isValid(level))
        return true;

    mHasTarget = false;
    return false;
}

void MoveToRandomBlockGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    // a fresh attempt forgets how often the last one got stuck
    mStuckTicks = 0;
    mStuckCount = 0;
    mLastPosition = mob.getPosition();
}

void MoveToRandomBlockGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mHasTarget = false;
}

void MoveToRandomBlockGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (!mHasTarget || !mob.getNavigation().isDone())
        return;

    const Vector3f position = mob.getPosition();
    const Vector3f delta(mTarget.x - position.x, mTarget.y - position.y, mTarget.z - position.z);
    const float distanceSquared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    if (distanceSquared < mWithinRadius * mWithinRadius) {
        mHasTarget = false;
        return;
    }

    if (mStuckCount >= STUCK_LIMIT) {
        mCooldown = STUCK_COOLDOWN_TICKS;
        return;
    }

    const float movedX = position.x - mLastPosition.x;
    const float movedY = position.y - mLastPosition.y;
    const float movedZ = position.z - mLastPosition.z;
    if (std::sqrt(movedX * movedX + movedY * movedY + movedZ * movedZ) < MOVED_DISTANCE) {
        ++mStuckTicks;
        ++mStuckCount;
    } else {
        mStuckTicks = 0;
    }
    mLastPosition = position;

    Vector3f destination = mTarget;
    // walked in legs of half the follow range; the game holds the squared distance against the range itself
    const float followRange = mob.getFollowRange();
    if (distanceSquared > followRange) {
        if (mStuckTicks < SIDESTEP_AFTER_TICKS) {
            const float distance = std::sqrt(distanceSquared);
            const float leg = followRange * 0.5f;
            destination = Vector3f(std::floor(position.x + std::floor(delta.x / distance * leg)),
                                   std::floor(position.y + std::floor(delta.y / distance * leg)),
                                   std::floor(position.z + std::floor(delta.z / distance * leg)));
        } else {
            const Vector3f offset = facingOffset(sidestepFacing(dominantFacing(delta), mRandom), SIDESTEP_DISTANCE);
            destination = Vector3f(std::floor(position.x + offset.x), std::floor(position.y + offset.y),
                                   std::floor(position.z + offset.z));
        }
    }

    mob.getNavigation().moveTo(destination, mSpeed);
}

bool MoveToRandomBlockGoal::_isValid(Level &level) const {
    const int32_t x = (int32_t) std::floor(mTarget.x);
    const int32_t y = (int32_t) std::floor(mTarget.y);
    const int32_t z = (int32_t) std::floor(mTarget.z);

    // an unloaded column is unknown, not free: the march waits for another pick
    const BlockState *above = level.peekBlockPtr(x, y + 1, z);
    const BlockState *at = level.peekBlockPtr(x, y, z);
    return above != nullptr && at != nullptr && above->mName == BlockNames::AIR && !BlockSupport::isSolid(*at);
}
