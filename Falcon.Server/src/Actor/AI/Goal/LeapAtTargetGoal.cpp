#include "Actor/AI/Goal/LeapAtTargetGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <random>

namespace {
    const float MIN_DISTANCE_SQUARED = 4.0f;
    const float MAX_DISTANCE_SQUARED = 16.0f;
    const int32_t CHANCE = 5;
    const float HORIZONTAL_STRENGTH = 0.4f;
    const float MOTION_KEPT = 0.2f;

    std::mt19937 &leapRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

LeapAtTargetGoal::LeapAtTargetGoal(float height, bool mustBeOnGround)
        : mHeight(height), mMustBeOnGround(mustBeOnGround) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Jump | (uint8_t) GoalControlFlag::Move);
}

bool LeapAtTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mMustBeOnGround && !mob.isOnGround())
        return false;

    const ServerPlayer *target = mob.getTarget(owner);
    if (target == nullptr)
        return false;

    const float distance = mob.distanceSquaredTo(*target);
    if (distance < MIN_DISTANCE_SQUARED || distance > MAX_DISTANCE_SQUARED)
        return false;

    return std::uniform_int_distribution<int32_t>(0, CHANCE - 1)(leapRandom()) == 0;
}

bool LeapAtTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return !mob.isOnGround();
}

void LeapAtTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *target = mob.getTarget(owner);
    if (target == nullptr)
        return;

    const Vector3f position = mob.getPosition();
    const Vector3f targetPosition = target->getPosition();
    float dx = targetPosition.x - position.x;
    float dz = targetPosition.z - position.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length > 0.0001f) {
        dx /= length;
        dz /= length;
    }

    const Vector3f motion = mob.getMotion();
    mob.setMotion(Vector3f(dx * HORIZONTAL_STRENGTH + motion.x * MOTION_KEPT, mHeight,
                           dz * HORIZONTAL_STRENGTH + motion.z * MOTION_KEPT));
}
