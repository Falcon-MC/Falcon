#include "Actor/AI/Goal/SwimIdleGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <random>

namespace {
    std::mt19937 &idleRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

SwimIdleGoal::SwimIdleGoal(int32_t idleTicks, float successRate) : mIdleTicks(idleTicks), mSuccessRate(successRate) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool SwimIdleGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return LiquidBlocksFetch::at(owner.getLevelFor(mob), mob.getPosition()).water
           && std::uniform_real_distribution<float>(0.0f, 1.0f)(idleRandom()) < mSuccessRate;
}

bool SwimIdleGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return mRemaining > 0;
}

void SwimIdleGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mRemaining = mIdleTicks;
    mob.getNavigation().stop(mob);
}

void SwimIdleGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    --mRemaining;
}
