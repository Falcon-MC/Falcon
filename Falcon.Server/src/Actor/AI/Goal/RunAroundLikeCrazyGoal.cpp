#include "Actor/AI/Goal/RunAroundLikeCrazyGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/RideSystem.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <random>

namespace {
    const int32_t HORIZONTAL_RANGE = 5;
    const int32_t VERTICAL_RANGE = 4;
    const int32_t TAME_ATTEMPT_CHANCE = 50;

    std::mt19937 &crazyRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t randomOffset(int32_t range) {
        return std::uniform_int_distribution<int32_t>(-range, range)(crazyRandom());
    }
}

RunAroundLikeCrazyGoal::RunAroundLikeCrazyGoal(float speed) : mSpeed(speed) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool RunAroundLikeCrazyGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (!mob.isMountTaming() || !mob.hasPassengers())
        return false;

    _pickTarget(mob);
    return true;
}

bool RunAroundLikeCrazyGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mob.isMountTaming() && mob.hasPassengers() && !mob.getNavigation().isDone();
}

void RunAroundLikeCrazyGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().moveTo(mTarget, mSpeed);
}

void RunAroundLikeCrazyGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}

void RunAroundLikeCrazyGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mob.isMountTaming() || !mob.hasPassengers())
        return;

    if (std::uniform_int_distribution<int32_t>(0, TAME_ATTEMPT_CHANCE - 1)(crazyRandom()) != 0)
        return;

    ServerPlayer *rider = dynamic_cast<ServerPlayer *>(RideSystem::resolve(owner, mob.getPassengers().front()));
    if (rider != nullptr)
        mob.attemptMountTame(owner, *rider);
}

void RunAroundLikeCrazyGoal::_pickTarget(const MobActor &mob) {
    const Vector3f position = mob.getPosition();
    mTarget = Vector3f(std::floor(position.x) + (float) randomOffset(HORIZONTAL_RANGE) + 0.5f,
                       std::floor(position.y) + (float) randomOffset(VERTICAL_RANGE),
                       std::floor(position.z) + (float) randomOffset(HORIZONTAL_RANGE) + 0.5f);
}
