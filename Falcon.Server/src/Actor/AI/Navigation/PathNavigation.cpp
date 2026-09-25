#include "Actor/AI/Navigation/PathNavigation.h"

#include "Actor/AI/Navigation/PathFinder.h"
#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const int32_t STUCK_TICKS = 40;
    const int32_t MAX_REPATHS = 3;
    const float STUCK_DISTANCE_SQUARED = 1.0e-4f;
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
        if (!PathFinder::tryReserveSearch(owner.getCurrentTick()))
            return;

        mNeedsPath = false;
        mob.getMoveControl().stop();
        if (!PathFinder::get().findPath(owner.getLevelFor(mob), mob, mTarget, mOptions, mPath)) {
            stop(mob);
            return;
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

    _checkStuck(mob);
}

void PathNavigation::_checkStuck(MobActor &mob) {
    const Vector3f position = mob.getPosition();
    const float dx = position.x - mLastPosition.x;
    const float dz = position.z - mLastPosition.z;
    mLastPosition = position;

    if (dx * dx + dz * dz > STUCK_DISTANCE_SQUARED) {
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
