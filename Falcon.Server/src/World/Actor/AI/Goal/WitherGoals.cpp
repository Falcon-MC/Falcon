#include "World/Actor/AI/Goal/WitherGoals.h"

#include "World/Actor/AI/Goal/RandomGroundPosition.h"
#include "World/Actor/Mob/Hostile/WitherActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

namespace {
    // the random stroll definition's own ranges
    const float DEFAULT_HORIZONTAL_RANGE = 10.0f;
    const float DEFAULT_VERTICAL_RANGE = 7.0f;
    // the follow range attribute the wither spawns with
    const float FOLLOW_RANGE = 70.0f;

    float numberOf(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    WitherActor *witherOf(MobActor &mob) {
        return dynamic_cast<WitherActor *>(&mob);
    }
}

WitherRandomAttackPosGoal::WitherRandomAttackPosGoal(float speed, int32_t horizontalRange, int32_t verticalRange)
        : mSpeed(speed), mHorizontalRange(horizontalRange), mVerticalRange(verticalRange),
          mRandom(std::random_device{}()) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool WitherRandomAttackPosGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    WitherActor *wither = witherOf(mob);
    Actor *target = mob.getTarget(owner);
    if (wither == nullptr || target == nullptr || mob.getFlags().get(ActorFlag::Powered)
        || !wither->isAerialAttackArmed() || mRunning)
        return false;

    Vector3i random;
    if (!RandomGroundPosition::find(owner.getLevelFor(mob), mob, mHorizontalRange, mVerticalRange, nullptr, mRandom,
                                    random))
        return false;

    // past the target, on the far side from the random point
    const Vector3f position = mob.getPosition();
    const float dx = (float) random.x - position.x;
    const float dz = (float) random.z - position.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    const float directionX = length < 0.0001f ? 0.0f : dx / length;
    const float directionZ = length < 0.0001f ? 0.0f : dz / length;
    const Vector3f other = target->getPosition();
    mWanted = Vector3f(other.x + directionX * (float) mHorizontalRange, position.y,
                       other.z + directionZ * (float) mHorizontalRange);
    return true;
}

bool WitherRandomAttackPosGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return !mob.getNavigation().isDone();
}

void WitherRandomAttackPosGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mRunning = true;
    if (WitherActor *wither = witherOf(mob))
        wither->setFlyingToAttackPosition(true);
    mob.getNavigation().stop(mob);
    mob.getNavigation().moveTo(mWanted, mSpeed);
}

void WitherRandomAttackPosGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mRunning = false;
    mob.getNavigation().stop(mob);
    if (WitherActor *wither = witherOf(mob))
        wither->endAerialAttack();
}

WitherTargetHighestDamageGoal::WitherTargetHighestDamageGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Target);
}

bool WitherTargetHighestDamageGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    WitherActor *wither = witherOf(mob);
    ServerPlayer *best = wither == nullptr ? nullptr : wither->findHighestDamagePlayer(owner);
    if (best == nullptr)
        return false;

    mTargetRuntimeId = best->getRuntimeId();
    return true;
}

bool WitherTargetHighestDamageGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    // a target lost for good is dropped; one outdone by another player only ends the goal
    Actor *target = mob.getTarget(owner);
    if (target == nullptr || !mob.canTarget(*target)
        || mob.distanceSquaredTo(*target) > FOLLOW_RANGE * FOLLOW_RANGE) {
        mob.clearTarget();
        return false;
    }

    WitherActor *wither = witherOf(mob);
    const ServerPlayer *best = wither == nullptr ? nullptr : wither->findHighestDamagePlayer(owner);
    return best != nullptr && best->getRuntimeId() == mTargetRuntimeId && target->getRuntimeId() == mTargetRuntimeId;
}

void WitherTargetHighestDamageGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mob.setTarget(owner, mTargetRuntimeId);
}

std::unique_ptr<Goal> WitherGoals::create(const MobActor &mob, const std::string &behavior,
                                          const json::Value &component) {
    if (dynamic_cast<const WitherActor *>(&mob) == nullptr)
        return nullptr;

    if (behavior == "wither_random_attack_pos_goal")
        return std::make_unique<WitherRandomAttackPosGoal>(
                mob.getMovementSpeed() * numberOf(component, "speed_multiplier", 1.0f),
                (int32_t) numberOf(component, "xz_dist", DEFAULT_HORIZONTAL_RANGE),
                (int32_t) numberOf(component, "y_dist", DEFAULT_VERTICAL_RANGE));

    if (behavior == "wither_target_highest_damage")
        return std::make_unique<WitherTargetHighestDamageGoal>();

    return nullptr;
}
