#include "Actor/AI/Goal/RamAttackGoal.h"

#include "Actor/AI/Goal/MobAttack.h"
#include "Actor/Definition/EntityEvents.h"
#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace {
    const int32_t PREPARE_TICKS = 20;
    const int32_t MAX_CHARGE_TICKS = 200;
    const int32_t REPATH_INTERVAL = 10;
    const float MIN_DIRECTION_LENGTH = 1.0e-4f;

    std::mt19937 &ramRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    void playSound(ServerNetworkHandler &owner, MobActor &mob, const std::string &sound) {
        if (!sound.empty())
            owner.playLevelSound(owner.getLevelFor(mob), sound, mob.getPosition(), mob.getIdentifier());
    }
}

RamAttackGoal::RamAttackGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool RamAttackGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return target != nullptr && target->isAlive() && owner.getCurrentTick() >= mNextRamTick;
}

bool RamAttackGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return !mDone && target != nullptr && target->isAlive();
}

void RamAttackGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mPhase = Phase::Approach;
    mPhaseTicks = 0;
    mDone = false;
    EntityEvents::fireTriggers(owner, mob, mSettings.mOnStart.get());
}

void RamAttackGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    const int32_t maximum = std::max(mSettings.mMinCooldownTicks, mSettings.mMaxCooldownTicks);
    mNextRamTick = owner.getCurrentTick()
                   + std::uniform_int_distribution<int32_t>(mSettings.mMinCooldownTicks, maximum)(ramRandom());
    mob.getNavigation().stop(mob);
    mob.getLookControl().clear();
}

void RamAttackGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;

    if (mPhase == Phase::Approach)
        _tickApproach(owner, mob, *target);
    else if (mPhase == Phase::Prepare)
        _tickPrepare(mob, *target);
    else
        _tickCharge(owner, mob, *target);
}

void RamAttackGoal::_tickApproach(ServerNetworkHandler &owner, MobActor &mob, const Actor &target) {
    const Vector3f position = mob.getPosition();
    const Vector3f targetPosition = target.getPosition();
    mob.getLookControl().setLookAt(targetPosition);

    const float dx = position.x - targetPosition.x;
    const float dz = position.z - targetPosition.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (distance >= mSettings.mMinRamDistance && distance <= mSettings.mRamDistance) {
        mob.getNavigation().stop(mob);
        mPhase = Phase::Prepare;
        mPhaseTicks = PREPARE_TICKS;
        playSound(owner, mob, mSettings.mPreRamSound);
        return;
    }

    if (--mPhaseTicks > 0)
        return;

    mPhaseTicks = REPATH_INTERVAL;
    const float wanted = std::min(std::max(distance, mSettings.mMinRamDistance), mSettings.mRamDistance);
    const float directionX = distance > MIN_DIRECTION_LENGTH ? dx / distance : 1.0f;
    const float directionZ = distance > MIN_DIRECTION_LENGTH ? dz / distance : 0.0f;
    mob.getNavigation().moveTo(Vector3f(targetPosition.x + directionX * wanted, targetPosition.y,
                                        targetPosition.z + directionZ * wanted), mSettings.mRunSpeed);
}

void RamAttackGoal::_tickPrepare(MobActor &mob, const Actor &target) {
    const Vector3f targetPosition = target.getPosition();
    mob.getLookControl().setLookAt(targetPosition);
    if (--mPhaseTicks > 0)
        return;

    const Vector3f position = mob.getPosition();
    const float dx = targetPosition.x - position.x;
    const float dz = targetPosition.z - position.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    mRamDirection = distance > MIN_DIRECTION_LENGTH ? Vector3f(dx / distance, 0.0f, dz / distance)
                                                    : Vector3f(1.0f, 0.0f, 0.0f);
    mob.getNavigation().moveTo(targetPosition, mSettings.mRamSpeed);
    mPhase = Phase::Charge;
    mPhaseTicks = 0;
}

void RamAttackGoal::_tickCharge(ServerNetworkHandler &owner, MobActor &mob, Actor &target) {
    if (MobAttack::isTouching(mob, target, 0.0f)) {
        MobAttack::hit(owner, mob, target, mob.getAttackDamage(owner.getProperties().getDifficulty()));
        owner.knockBack(target, mRamDirection.x, mRamDirection.z, mSettings.mKnockbackForce,
                        mSettings.mKnockbackHeight);
        playSound(owner, mob, mSettings.mRamImpactSound);
        mDone = true;
        return;
    }

    if (++mPhaseTicks > MAX_CHARGE_TICKS || mob.getNavigation().isDone())
        mDone = true;
}
