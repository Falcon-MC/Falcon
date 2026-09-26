#include "Actor/AI/Goal/SlimeGoals.h"

#include "Actor/Mob/Hostile/AbstractSlimeActor.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Core/Math/MathConstants.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <random>

namespace {
    const float FLOAT_JUMP_CHANCE = 0.8f;
    const float FLOAT_SPEED = 1.2f;
    const float KEEP_JUMPING_SPEED = 1.0f;
    const int32_t RANDOM_DIRECTION_MIN_TICKS = 40;
    const int32_t RANDOM_DIRECTION_EXTRA_TICKS = 60;
    const int32_t ATTACK_TIRED_TICKS = 300;
    const float FULL_TURN = 360.0f;

    std::mt19937 &slimeGoalRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    bool inLiquid(ServerNetworkHandler &owner, const MobActor &mob) {
        const LiquidContact contact = LiquidBlocksFetch::at(owner.getLevelFor(mob), mob.getPosition());
        return contact.water || contact.lava;
    }
}

AbstractSlimeActor *SlimeGoal::slimeOf(MobActor &mob) {
    return dynamic_cast<AbstractSlimeActor *>(&mob);
}

SlimeFloatGoal::SlimeFloatGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Jump | (uint8_t) GoalControlFlag::Move);
}

bool SlimeFloatGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return slimeOf(mob) != nullptr && inLiquid(owner, mob);
}

void SlimeFloatGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    AbstractSlimeActor *slime = slimeOf(mob);
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(slimeGoalRandom()) < FLOAT_JUMP_CHANCE)
        slime->hop();
    slime->setHopSpeed(FLOAT_SPEED);
}

SlimeKeepOnJumpingGoal::SlimeKeepOnJumpingGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Jump | (uint8_t) GoalControlFlag::Move);
}

bool SlimeKeepOnJumpingGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return slimeOf(mob) != nullptr && !mob.isRiding();
}

void SlimeKeepOnJumpingGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    slimeOf(mob)->setHopSpeed(KEEP_JUMPING_SPEED);
}

SlimeRandomDirectionGoal::SlimeRandomDirectionGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Look);
}

bool SlimeRandomDirectionGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return slimeOf(mob) != nullptr && mob.getTarget(owner) == nullptr && (mob.isOnGround() || inLiquid(owner, mob));
}

void SlimeRandomDirectionGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (--mNextChange > 0)
        return;

    mNextChange = RANDOM_DIRECTION_MIN_TICKS
                  + std::uniform_int_distribution<int32_t>(0, RANDOM_DIRECTION_EXTRA_TICKS - 1)(slimeGoalRandom());
    const float yaw = std::uniform_real_distribution<float>(0.0f, FULL_TURN)(slimeGoalRandom());
    slimeOf(mob)->setHopDirection(yaw, false);
}

SlimeAttackGoal::SlimeAttackGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Look);
}

bool SlimeAttackGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return slimeOf(mob) != nullptr && target != nullptr && target->isAlive();
}

bool SlimeAttackGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return canUse(owner, mob) && mTiredTicks > 0;
}

void SlimeAttackGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mTiredTicks = ATTACK_TIRED_TICKS;
}

void SlimeAttackGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;

    --mTiredTicks;
    const Vector3f position = mob.getPosition();
    const Vector3f targetPosition = target->getPosition();
    const float yaw = std::atan2(-(targetPosition.x - position.x), targetPosition.z - position.z)
                      / MathConstants::DEGREES_TO_RADIANS_F;
    mob.getLookControl().setLookAt(targetPosition);
    slimeOf(mob)->setHopDirection(yaw, true);
}
