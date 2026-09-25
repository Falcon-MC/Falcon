#include "Actor/AI/Goal/RandomLookAroundGoal.h"

#include "Actor/Mob/MobActor.h"

#include <cmath>
#include <random>

namespace {
    const float START_CHANCE = 0.02f;
    const int32_t MIN_LOOK_TIME = 20;
    const int32_t EXTRA_LOOK_TIME = 20;
    const float TWO_PI = 6.2831855f;

    std::mt19937 &lookAroundRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

RandomLookAroundGoal::RandomLookAroundGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool RandomLookAroundGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(lookAroundRandom()) < START_CHANCE;
}

bool RandomLookAroundGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return mLookTime >= 0;
}

void RandomLookAroundGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    const float angle = std::uniform_real_distribution<float>(0.0f, TWO_PI)(lookAroundRandom());
    mOffsetX = std::cos(angle);
    mOffsetZ = std::sin(angle);
    mLookTime = MIN_LOOK_TIME + std::uniform_int_distribution<int32_t>(0, EXTRA_LOOK_TIME - 1)(lookAroundRandom());
}

void RandomLookAroundGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mLookTime--;

    const Vector3f position = mob.getPosition();
    mob.getLookControl().setLookAt(Vector3f(position.x + mOffsetX, position.y + mob.getSize().mHeight * 0.85f,
                                            position.z + mOffsetZ));
}
