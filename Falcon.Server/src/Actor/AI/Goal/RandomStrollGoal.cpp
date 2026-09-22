#include "Actor/AI/Goal/RandomStrollGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <random>

namespace {
    std::mt19937 &strollRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

RandomStrollGoal::RandomStrollGoal(float speed, int32_t range, int32_t interval, bool avoidWater,
                                   int32_t maxRetries)
        : mSpeed(speed), mRange(range), mInterval(interval), mAvoidWater(avoidWater), mMaxRetries(maxRetries) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool RandomStrollGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return true;
}

void RandomStrollGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mTicksSinceTarget = mInterval;
}

void RandomStrollGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mTicksSinceTarget = 0;
}

void RandomStrollGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    mTicksSinceTarget++;
    if (!shouldPickTarget(mob))
        return;

    Level &level = owner.getLevelFor(mob);
    Vector3f target = _randomTarget(mob);
    if (mAvoidWater) {
        for (int32_t retry = 0; retry <= mMaxRetries && _isAboveWater(level, target); ++retry)
            target = _randomTarget(mob);
    }

    mob.getNavigation().moveTo(target, mSpeed);
    mTicksSinceTarget = 0;
}

bool RandomStrollGoal::shouldPickTarget(MobActor &mob) const {
    (void) mob;
    return mTicksSinceTarget >= mInterval;
}

Vector3f RandomStrollGoal::_randomTarget(const MobActor &mob) const {
    std::uniform_int_distribution<int32_t> offset(-mRange, mRange - 1);
    const Vector3f position = mob.getPosition();
    const float x = std::floor(position.x) + (float) offset(strollRandom());
    const float z = std::floor(position.z) + (float) offset(strollRandom());
    return Vector3f(x, position.y, z);
}

bool RandomStrollGoal::_isAboveWater(Level &level, const Vector3f &target) const {
    return LiquidBlocksFetch::at(level, Vector3f(target.x + 0.5f, target.y - 0.5f, target.z + 0.5f)).water;
}
