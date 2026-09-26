#include "Actor/AI/Goal/StompTurtleEggGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/Blocks/TurtleEggBlock.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    const int32_t STOMP_INTERVAL = 20;
    const int32_t REPATH_INTERVAL = 10;

    std::mt19937 &stompRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y, (float) position.z + 0.5f);
    }
}

StompTurtleEggGoal::StompTurtleEggGoal(float speed, int32_t searchRange, int32_t searchHeight, float goalRadius,
                                       int32_t interval)
        : mSpeed(speed), mSearchRange(searchRange), mSearchHeight(searchHeight), mGoalRadius(goalRadius),
          mInterval(std::max(1, interval)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool StompTurtleEggGoal::_isEgg(ServerNetworkHandler &owner, const MobActor &mob, const Vector3i &position) const {
    const BlockState *state = owner.getLevelFor(mob).peekBlockPtr(position.x, position.y, position.z);
    return state != nullptr && TurtleEggBlock::matches(state->mName);
}

bool StompTurtleEggGoal::_findEgg(ServerNetworkHandler &owner, const MobActor &mob, Vector3i &egg) const {
    const Vector3f position = mob.getPosition();
    const int32_t originX = (int32_t) std::floor(position.x);
    const int32_t originY = (int32_t) std::floor(position.y);
    const int32_t originZ = (int32_t) std::floor(position.z);

    float nearest = -1.0f;
    for (int32_t x = originX - mSearchRange; x <= originX + mSearchRange; ++x) {
        for (int32_t y = originY - mSearchHeight; y <= originY + mSearchHeight; ++y) {
            for (int32_t z = originZ - mSearchRange; z <= originZ + mSearchRange; ++z) {
                const Vector3i candidate(x, y, z);
                if (!_isEgg(owner, mob, candidate))
                    continue;

                const Vector3f center = centerOf(candidate);
                const float dx = center.x - position.x;
                const float dy = center.y - position.y;
                const float dz = center.z - position.z;
                const float distance = dx * dx + dy * dy + dz * dz;
                if (nearest < 0.0f || distance < nearest) {
                    nearest = distance;
                    egg = candidate;
                }
            }
        }
    }
    return nearest >= 0.0f;
}

bool StompTurtleEggGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (std::uniform_int_distribution<int32_t>(0, mInterval - 1)(stompRandom()) != 0)
        return false;
    return _findEgg(owner, mob, mEgg);
}

bool StompTurtleEggGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return _isEgg(owner, mob, mEgg);
}

void StompTurtleEggGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mStompTicks = 0;
    mRepathTicks = 0;
    mob.getNavigation().moveTo(centerOf(mEgg), mSpeed);
}

void StompTurtleEggGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}

void StompTurtleEggGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Vector3f target = centerOf(mEgg);
    const Vector3f position = mob.getPosition();
    const float dx = target.x - position.x;
    const float dy = target.y - position.y;
    const float dz = target.z - position.z;
    if (dx * dx + dy * dy + dz * dz > mGoalRadius * mGoalRadius) {
        mStompTicks = 0;
        if (--mRepathTicks <= 0) {
            mRepathTicks = REPATH_INTERVAL;
            mob.getNavigation().moveTo(target, mSpeed);
        }
        return;
    }

    if (++mStompTicks < STOMP_INTERVAL)
        return;

    mStompTicks = 0;
    Level &level = owner.getLevelFor(mob);
    const BlockState *state = level.peekBlockPtr(mEgg.x, mEgg.y, mEgg.z);
    if (state == nullptr)
        return;

    const BlockState egg = *state;
    TurtleEggBlock::breakEgg(owner, level, mEgg, egg);
}
