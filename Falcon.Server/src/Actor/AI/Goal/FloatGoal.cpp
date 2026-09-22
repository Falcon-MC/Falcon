#include "Actor/AI/Goal/FloatGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <random>

namespace {
    const float EYE_HEIGHT_RATIO = 0.85f;
    const float STRONG_BOB_FACTOR = 0.8f;
    const float BOB_FACTOR = 0.6f;
    const int32_t STRONG_BOB_ROLLS = 4;

    std::mt19937 &floatRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

FloatGoal::FloatGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Jump);
}

bool FloatGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return true;
}

void FloatGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Level &level = owner.getLevelFor(mob);
    const Vector3f position = mob.getPosition();
    const float floatingHeight = mob.getSize().mHeight * EYE_HEIGHT_RATIO;
    const bool eyesInWater = LiquidBlocksFetch::at(level, Vector3f(position.x, position.y + floatingHeight,
                                                                   position.z)).water;

    if (eyesInWater) {
        mEyesWereInWater = true;
        return;
    }

    if (!mEyesWereInWater)
        return;

    mEyesWereInWater = false;
    if (!LiquidBlocksFetch::at(level, position).water)
        return;

    const bool strong = std::uniform_int_distribution<int32_t>(0, STRONG_BOB_ROLLS - 1)(floatRandom()) == STRONG_BOB_ROLLS - 1;
    Vector3f motion = mob.getMotion();
    motion.y += floatingHeight * (strong ? STRONG_BOB_FACTOR : BOB_FACTOR);
    mob.setMotion(motion);
}
