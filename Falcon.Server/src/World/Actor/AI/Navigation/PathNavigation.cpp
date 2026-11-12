#include "World/Actor/AI/Navigation/PathNavigation.h"

#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "Network/ServerNetworkHandler.h"
#include "Core/Math/VectorMath.h"

#include <cmath>

namespace {
    const int32_t STUCK_TICKS = 40;
    const int32_t MAX_REPATHS = 3;
    const float STUCK_DISTANCE_SQUARED = 1.0e-4f;
    // a partial path must bring the mob at least this much closer to an unreachable target
    const float PARTIAL_PATH_MIN_GAIN = 1.0f;

    using VectorMath::distanceSquared;
}

void PathNavigation::moveTo(const Vector3f &target, float speed) {
    mTarget = target;
    mSpeed = speed;
    mHasTarget = true;
    mNeedsPath = true;
    mStuckTicks = 0;
    mRepaths = 0;
}

void PathNavigation::stop(MobActor &mob) {
    mHasTarget = false;
    mNeedsPath = false;
    mPath.clear();
    mob.getMoveControl().stop();
}

void PathNavigation::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mHasTarget)
        return;

    if (mNeedsPath) {
        Level &level = owner.getLevelFor(mob);
        const bool direct = PathFinder::get().findDirectPath(level, mob, mTarget, mOptions, mPath);
        if (!direct && !PathFinder::tryReserveSearch(owner.getCurrentTick()))
            return;

        mNeedsPath = false;
        mob.getMoveControl().stop();
        if (!direct && !PathFinder::get().findPath(level, mob, mTarget, mOptions, mPath)) {
            stop(mob);
            return;
        }

        // out of reach (above a wall, across water): at the closest point a new partial path only sends the mob
        // back and forth; wait for the target to move
        if (!mPath.reachesTarget()) {
            const float here = std::sqrt(distanceSquared(mob.getPosition(), mTarget));
            const float there = std::sqrt(distanceSquared(mPath.last(), mTarget));
            if (there > here - PARTIAL_PATH_MIN_GAIN) {
                stop(mob);
                return;
            }
        }

        mLastPosition = mob.getPosition();
        mStuckTicks = 0;
    }

    if (!mob.getMoveControl().hasWanted()) {
        if (mPath.isDone()) {
            stop(mob);
            return;
        }

        mob.getMoveControl().setWantedPosition(mPath.current(), mSpeed);
        mPath.advance();
    }

    const bool freeMoving = mOptions.isVolumetric()
                            || (mOptions.swimsThroughWater()
                                && LiquidBlocksFetch::at(owner.getLevelFor(mob), mob.getPosition()).water);
    _checkStuck(mob, freeMoving);
}

void PathNavigation::_checkStuck(MobActor &mob, bool freeMoving) {
    const Vector3f position = mob.getPosition();
    const float dx = position.x - mLastPosition.x;
    const float dy = freeMoving ? position.y - mLastPosition.y : 0.0f;
    const float dz = position.z - mLastPosition.z;
    mLastPosition = position;

    if (dx * dx + dy * dy + dz * dz > STUCK_DISTANCE_SQUARED) {
        mStuckTicks = 0;
        return;
    }

    if (++mStuckTicks < STUCK_TICKS)
        return;

    mStuckTicks = 0;
    if (++mRepaths > MAX_REPATHS) {
        stop(mob);
        return;
    }
    mNeedsPath = true;
}
