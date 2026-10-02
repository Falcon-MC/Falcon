#include "Actor/AI/Goal/GoHomeGoal.h"

#include "Actor/Definition/EntityEvents.h"
#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <random>
#include <utility>

namespace {
    std::mt19937 &goHomeRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

GoHomeGoal::GoHomeGoal(float speed, int32_t interval, float goalRadius, std::shared_ptr<json::Value> onHome,
                       std::shared_ptr<json::Value> onFailed)
        : mSpeed(speed), mInterval(interval), mGoalRadius(goalRadius), mOnHome(std::move(onHome)),
          mOnFailed(std::move(onFailed)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool GoHomeGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (!mob.hasHome())
        return false;

    return mInterval <= 1 || std::uniform_int_distribution<int32_t>(0, mInterval - 1)(goHomeRandom()) == 0;
}

bool GoHomeGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return !mFinished && mob.hasHome();
}

void GoHomeGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mFinished = false;
    mob.getNavigation().moveTo(mob.getHomePosition(), mSpeed);
}

void GoHomeGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mFinished = false;
}

void GoHomeGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (_isHome(mob)) {
        mFinished = true;
        mob.getNavigation().stop(mob);
        _fire(owner, mob, mOnHome.get());
        return;
    }

    if (!mob.getNavigation().isDone())
        return;

    mFinished = true;
    _fire(owner, mob, mOnFailed.get());
}

bool GoHomeGoal::_isHome(const MobActor &mob) const {
    const Vector3f position = mob.getPosition();
    const Vector3f &home = mob.getHomePosition();
    const float dx = position.x - home.x;
    const float dy = position.y - home.y;
    const float dz = position.z - home.z;
    return dx * dx + dy * dy + dz * dz <= mGoalRadius * mGoalRadius;
}

void GoHomeGoal::_fire(ServerNetworkHandler &owner, MobActor &mob, const json::Value *triggers) {
    if (triggers == nullptr)
        return;

    const Vector3f &home = mob.getHomePosition();
    mob.setEventBlock(Vector3i((int32_t) std::floor(home.x), (int32_t) std::floor(home.y),
                               (int32_t) std::floor(home.z)));
    EntityEvents::fireTriggers(owner, mob, triggers);
    mob.clearEventBlock();
}
