#include "World/Actor/AI/Goal/BaseMoveToBlockGoal.h"

#include "World/Actor/AI/Goal/RandomGroundPosition.h"
#include "World/Actor/AI/Navigation/Path.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <random>

namespace {
    // the block a path ends on: a walking path stops on the middle of it, the target is its corner
    Vector3f endBlockOf(const Path &path) {
        const Vector3f &last = path.last();
        return Vector3f(std::floor(last.x), std::floor(last.y), std::floor(last.z));
    }

    const int32_t MAX_TRAVEL_TICKS = 1200;
    const int32_t MAX_STAY_TICKS = 1200;
    // the walk repaths when its path runs out, else when the travel ticks fall on a multiple of a number rolled
    // anew each tick between 200 and 399; the searches wait as long
    const int32_t REPATH_INTERVAL = 200;
    const float NO_TARGET_DISTANCE = 99999.99f;

    std::mt19937 &moveRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    Vector3i floored(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }

    // 0, 1, -1, 2, -2, ...
    int32_t nextOffset(int32_t offset) {
        return (offset <= 0 ? 1 : 0) - offset;
    }

    float squaredDistance(const Vector3f &first, const Vector3f &second) {
        const float dx = first.x - second.x;
        const float dy = first.y - second.y;
        const float dz = first.z - second.z;
        return dx * dx + dy * dy + dz * dz;
    }

    Vector3f centerOf(const MobActor &mob) {
        const Vector3f &position = mob.getPosition();
        return Vector3f(position.x, position.y + mob.getSize().mHeight * 0.5f, position.z);
    }
}

BaseMoveToBlockGoal::BaseMoveToBlockGoal(Settings settings) : mSettings(settings) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Jump);
}

bool BaseMoveToBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mGiveUpLeft > 0) {
        --mGiveUpLeft;
        return false;
    }
    if (mSearchCooldown > 0) {
        --mSearchCooldown;
        return false;
    }
    mSearchCooldown = REPATH_INTERVAL + std::uniform_int_distribution<int32_t>(0, REPATH_INTERVAL - 1)(moveRandom());
    return findTarget(owner, mob);
}

Vector3f BaseMoveToBlockGoal::targetPoint() const {
    return Vector3f((float) mTarget.x, (float) mTarget.y + 1.0f, (float) mTarget.z);
}

void BaseMoveToBlockGoal::moveToTarget(MobActor &mob) {
    mob.getNavigation().moveTo(targetPoint(), mSettings.mSpeed);
}

bool BaseMoveToBlockGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const float radiusSquared = mSettings.mGoalRadius * mSettings.mGoalRadius;
    const Path *path = mob.getNavigation().getPath();
    if (path != nullptr && !path->isEmpty() && squaredDistance(endBlockOf(*path), targetPoint()) > radiusSquared)
        return false;
    if (mGiveUpLeft != 0 || mStayTicks > MAX_STAY_TICKS || mTravelTicks > MAX_TRAVEL_TICKS)
        return false;
    return isValidTarget(owner, mob, mTarget);
}

void BaseMoveToBlockGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    moveToTarget(mob);
    mLastPathStart = floored(mob.getPosition());
    mTravelTicks = 0;
    mStayTicks = 0;
}

void BaseMoveToBlockGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mTravelTicks = 0;
}

void BaseMoveToBlockGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (squaredDistance(targetPoint(), centerOf(mob)) <= mSettings.mGoalRadius * mSettings.mGoalRadius) {
        mReached = true;
        ++mStayTicks;
        return;
    }

    mReached = false;
    ++mTravelTicks;
    const int32_t interval = REPATH_INTERVAL
                             + std::uniform_int_distribution<int32_t>(0, REPATH_INTERVAL - 1)(moveRandom());
    if (!mob.getNavigation().isDone() && mTravelTicks % interval != 0)
        return;

    moveToTarget(mob);
    // still on the block it set off from at the last repath: stuck
    const Vector3i here = floored(mob.getPosition());
    if (here == mLastPathStart)
        mGiveUpLeft = mSettings.mGiveUpTicks;
    mLastPathStart = here;
}

bool BaseMoveToBlockGoal::_canReach(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) const {
    const Vector3f point((float) position.x, (float) position.y + 1.0f, (float) position.z);
    Path path;
    if (!PathFinder::get().findPath(owner.getLevelFor(mob), mob, point, mob.getNavigation().getOptions(), path)
        || path.isEmpty())
        return false;
    return squaredDistance(endBlockOf(path), point) <= mSettings.mGoalRadius * mSettings.mGoalRadius;
}

bool BaseMoveToBlockGoal::_tryCandidate(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &origin,
                                        const Vector3i &position, float &best) {
    if (!isValidTarget(owner, mob, position))
        return false;

    const float dx = (float) (origin.x - position.x);
    const float dy = (float) (origin.y - position.y);
    const float dz = (float) (origin.z - position.z);
    const float distance = dx * dx + dy * dy + dz * dz;
    if (distance >= best || !_canReach(owner, mob, position))
        return false;

    best = distance;
    mTarget = position;
    return true;
}

bool BaseMoveToBlockGoal::findTarget(ServerNetworkHandler &owner, MobActor &mob) {
    const Vector3i origin = floored(mob.getPosition());
    float best = NO_TARGET_DISTANCE;
    bool found = false;

    if (mSettings.mSearchCount > 0) {
        Level &level = owner.getLevelFor(mob);
        for (int32_t attempt = 0; attempt < mSettings.mSearchCount; ++attempt) {
            Vector3i ground;
            if (!RandomGroundPosition::find(level, mob, mSettings.mSearchRange, mSettings.mSearchHeight, nullptr,
                                            moveRandom(), ground))
                continue;
            found |= _tryCandidate(owner, mob, origin, Vector3i(ground.x, ground.y - 1, ground.z), best);
        }
        return found;
    }

    if (mSettings.mSearchHeight < 0 || mSettings.mSearchRange <= 0)
        return false;

    // the layers from the one under the feet outwards, each scanned ring by ring from the mob
    const float radiusSquared = mSettings.mGoalRadius * mSettings.mGoalRadius;
    for (int32_t dy = 0; dy <= mSettings.mSearchHeight; dy = nextOffset(dy)) {
        const int32_t y = origin.y - 1 + dy;
        for (int32_t ring = 0; ring < mSettings.mSearchRange; ++ring) {
            for (int32_t dx = 0; dx <= ring; dx = nextOffset(dx)) {
                const bool edge = dx <= -ring || dx >= ring;
                for (int32_t dz = edge ? 0 : ring; dz <= ring; dz = nextOffset(dz)) {
                    if (!_tryCandidate(owner, mob, origin, Vector3i(origin.x + dx, y, origin.z + dz), best))
                        continue;
                    found = true;
                    if (best <= radiusSquared)
                        return true;
                }
            }
        }
    }
    return found;
}
