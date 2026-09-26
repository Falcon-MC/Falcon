#include "Actor/AI/Goal/RandomSwimGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    const int32_t TARGET_ATTEMPTS = 10;

    std::mt19937 &swimRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t randomOffset(int32_t range) {
        return range <= 0 ? 0 : std::uniform_int_distribution<int32_t>(-range, range)(swimRandom());
    }
}

RandomSwimGoal::RandomSwimGoal(float speed, int32_t horizontalRange, int32_t verticalRange, int32_t interval)
        : mSpeed(speed), mHorizontalRange(std::max(1, horizontalRange)), mVerticalRange(std::max(0, verticalRange)),
          mInterval(std::max(1, interval)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool RandomSwimGoal::_findWater(ServerNetworkHandler &owner, const MobActor &mob, Vector3f &target) const {
    Level &level = owner.getLevelFor(mob);
    const Vector3f position = mob.getPosition();
    for (int32_t attempt = 0; attempt < TARGET_ATTEMPTS; ++attempt) {
        const Vector3f candidate(std::floor(position.x) + (float) randomOffset(mHorizontalRange) + 0.5f,
                                 std::floor(position.y) + (float) randomOffset(mVerticalRange),
                                 std::floor(position.z) + (float) randomOffset(mHorizontalRange) + 0.5f);
        if (LiquidBlocksFetch::at(level, candidate).water) {
            target = candidate;
            return true;
        }
    }
    return false;
}

bool RandomSwimGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!LiquidBlocksFetch::at(owner.getLevelFor(mob), mob.getPosition()).water)
        return false;
    if (std::uniform_int_distribution<int32_t>(0, mInterval - 1)(swimRandom()) != 0)
        return false;

    return _findWater(owner, mob, mTarget);
}

bool RandomSwimGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return !mob.getNavigation().isDone();
}

void RandomSwimGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().moveTo(mTarget, mSpeed);
}

void RandomSwimGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}
