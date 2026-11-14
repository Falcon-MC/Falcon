#include "World/Actor/AI/Goal/MoveThroughVillageGoal.h"

#include "World/Actor/AI/Goal/RandomGroundPosition.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <limits>
#include <random>

namespace {
    // the goal's fixed search: ten tries in 15 blocks around, 5 up or down
    const int32_t SEARCH_COUNT = 10;
    const int32_t SEARCH_HORIZONTAL = 15;
    const int32_t SEARCH_VERTICAL = 5;
    // a fresh start every 200 to 399 ticks, a new point every 100 to 199 ticks while walking
    const int32_t START_INTERVAL = 200;
    const int32_t SEARCH_INTERVAL = 100;
    const int32_t GIVE_UP_TICKS = 1200;
    const int64_t NIGHT_START = 12000;

    std::mt19937 &villageWalkRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t upTo(int32_t bound) {
        return std::uniform_int_distribution<int32_t>(0, bound - 1)(villageWalkRandom());
    }

    bool isNight(Level &level) {
        const int64_t time = ((level.getTime() % Level::TICKS_PER_DAY) + Level::TICKS_PER_DAY) % Level::TICKS_PER_DAY;
        return time >= NIGHT_START;
    }
}

MoveThroughVillageGoal::MoveThroughVillageGoal(float speed, bool onlyAtNight)
        : mSpeed(speed), mOnlyAtNight(onlyAtNight) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool MoveThroughVillageGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mIntervalLeft > 0) {
        --mIntervalLeft;
        return false;
    }
    mIntervalLeft = START_INTERVAL + upTo(START_INTERVAL);

    if (mOnlyAtNight && !isNight(owner.getLevelFor(mob)))
        return false;
    return _findTarget(owner, mob);
}

bool MoveThroughVillageGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mOnlyAtNight && !isNight(owner.getLevelFor(mob)))
        return false;
    return mTicksMoving <= GIVE_UP_TICKS && !mob.getNavigation().isDone();
}

void MoveThroughVillageGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mTicksMoving = 0;
    mNextSearchTick = owner.getCurrentTick() + SEARCH_INTERVAL + upTo(SEARCH_INTERVAL);
    mob.getNavigation().moveTo(mTarget, mSpeed);
}

void MoveThroughVillageGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}

void MoveThroughVillageGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    ++mTicksMoving;
    if (owner.getCurrentTick() <= mNextSearchTick)
        return;

    if (_findTarget(owner, mob))
        mob.getNavigation().moveTo(mTarget, mSpeed);
    mNextSearchTick = owner.getCurrentTick() + SEARCH_INTERVAL + upTo(SEARCH_INTERVAL);
}

bool MoveThroughVillageGoal::_findTarget(ServerNetworkHandler &owner, const MobActor &mob) {
    Level &level = owner.getLevelFor(mob);
    const Vector3f &position = mob.getPosition();
    const int32_t originX = (int32_t) std::floor(position.x);
    const int32_t originY = (int32_t) std::floor(position.y);
    const int32_t originZ = (int32_t) std::floor(position.z);
    float best = std::numeric_limits<float>::max();
    bool found = false;
    for (int32_t search = 0; search < SEARCH_COUNT; ++search) {
        Vector3i standing;
        if (!RandomGroundPosition::find(level, mob, SEARCH_HORIZONTAL, SEARCH_VERTICAL, nullptr, villageWalkRandom(),
                                        standing))
            continue;

        // the point is the ground block; the block above it must be open to the sky
        const Vector3i ground(standing.x, standing.y - 1, standing.z);
        if (level.getHeightAt(ground.x, ground.z) > ground.y + 1)
            continue;

        const float dx = (float) (originX - ground.x);
        const float dy = (float) (originY - ground.y);
        const float dz = (float) (originZ - ground.z);
        const float distance = dx * dx + dy * dy + dz * dz;
        if (distance >= best)
            continue;

        best = distance;
        mTarget = Vector3f((float) ground.x, (float) standing.y, (float) ground.z);
        found = true;
    }
    return found;
}
