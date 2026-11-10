#include "World/Actor/AI/Goal/RangedAttackGoal.h"

#include "World/Actor/AI/Control/LookControl.h"
#include "World/Actor/AI/Goal/ChargeHeldItemGoal.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Mob/Component/MobShooter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Item/CrossbowItem.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
    const float RADIANS_TO_DEGREES = 57.295776f;
    const float DEGREES_TO_RADIANS = 0.017453292f;
    const float BACK_AWAY_MARGIN = 0.5f;
    const float EYE_HEIGHT_RATIO = 0.85f;
    const char *const SHOOTER_COMPONENT = "minecraft:shooter";
    // level sound event 44, played as the charge reaches charge_charged_trigger
    const char *const CHARGED_SOUND = "warn";

    void setFlag(ServerNetworkHandler &owner, MobActor &mob, ActorFlag flag, bool value) {
        if (mob.getFlags().get(flag) == value)
            return;

        mob.getFlags().set(flag, value);
        owner.syncActorFlags(mob);
    }

    bool holdsUnchargedCrossbow(const MobActor &mob) {
        return ChargeHeldItemGoal::holdsCrossbow(mob) && !mob.getFlags().get(ActorFlag::Charged);
    }
}

RangedAttackGoal::RangedAttackGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

// needs a navigation component; an empty crossbow waits for charge_held_item: sub_14D4297E0
bool RangedAttackGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mSettings.mHasNavigation || !_hasShooter(mob) || holdsUnchargedCrossbow(mob))
        return false;

    const Actor *target = mob.getTarget(owner);
    if (target == nullptr || !target->isAlive())
        return false;

    if (mSettings.mSetPersistent && target->isPlayer())
        mob.setPersistent(true);
    return true;
}

// a path still being walked keeps the goal: sub_14D429A50
bool RangedAttackGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return !mob.getNavigation().isDone() || canUse(owner, mob);
}

// the definition only changes with component groups: resolve the projectile once per shooter component
bool RangedAttackGoal::_hasShooter(const MobActor &mob) {
    const json::Value *shooter = mob.getComponent(SHOOTER_COMPONENT);
    if (shooter == mShooter)
        return mShooterUsable;

    mShooter = shooter;
    const json::Value *identifier = shooter == nullptr ? nullptr : shooter->get("def");
    mShooterUsable = identifier != nullptr && identifier->isString()
                     && ActorClassRegistry::getPrototype(identifier->mString) != nullptr;
    return mShooterUsable;
}

void RangedAttackGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mHeldCharged = ChargeHeldItemGoal::holdsCrossbow(mob) && mob.getFlags().get(ActorFlag::Charged);
    mob.getLookControl().setPitchEnabled(true);
    setFlag(owner, mob, ActorFlag::FacingTargetToRangeAttack, true);
}

// the charged flag stays on a loaded crossbow: sub_14D429CB0
void RangedAttackGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    mSeeTime = 0;
    mChargeTime = 0;
    if (!mHeldCharged)
        setFlag(owner, mob, ActorFlag::Charged, false);
    mHeldCharged = false;
    mHasPathTarget = false;
    mob.getNavigation().stop(mob);
    mob.getLookControl().clear();
    mob.getLookControl().setPitchEnabled(false);
    setFlag(owner, mob, ActorFlag::FacingTargetToRangeAttack, false);
}

// sub_14D429E10
void RangedAttackGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    if (target == nullptr || !target->isAlive())
        return;

    const float distanceSquared = mob.distanceSquaredTo(*target);
    const bool canSee = mob.canSee(owner, *target);
    mSeeTime = canSee ? mSeeTime + 1 : 0;

    _move(mob, *target, distanceSquared);

    const Vector3f targetPosition = target->getPosition();
    mob.getLookControl().setLookAt(targetPosition);

    if (mBurstRemaining > 0) {
        _tickBurst(owner, mob, *target, distanceSquared, canSee);
        return;
    }

    if (mSettings.mChargeShootTriggerTicks > 0) {
        _tickCharge(owner, mob, canSee);
        return;
    }

    const int32_t previous = mAttackTime--;
    if (previous == 1) {
        mBurstRemaining = mSettings.mBurstShots;
        mBurstCooldown = 0;
    } else if (previous <= 0) {
        mAttackTime = _intervalFor(distanceSquared);
    }
}

void RangedAttackGoal::_move(MobActor &mob, const Actor &target, float distanceSquared) {
    PathNavigation &navigation = mob.getNavigation();
    const float radiusMin = mSettings.mAttackRadiusMin;
    const Vector3f position = mob.getPosition();
    const Vector3f targetPosition = target.getPosition();

    if (distanceSquared < radiusMin * radiusMin) {
        // radius_min + 0.5 away; the game feeds atan2(dz, dx) to sin for x and cos for z, axes swapped as it does
        const float distance = (radiusMin - std::sqrt(distanceSquared)) + BACK_AWAY_MARGIN;
        const float angle = std::atan2(position.z - targetPosition.z, position.x - targetPosition.x);
        navigation.moveTo(
                Vector3f(position.x + std::sin(angle) * distance, position.y, position.z + std::cos(angle) * distance),
                mSettings.mSpeed);
        mHasPathTarget = false;
        return;
    }

    const float radius = mSettings.mAttackRadius;
    const float followRadius = radiusMin > 0.0f ? (radiusMin + BACK_AWAY_MARGIN) * (radiusMin + BACK_AWAY_MARGIN)
                                                : 0.0f;
    if (distanceSquared > radius * radius || mSeeTime < mSettings.mTargetInSightTicks
        || (mSettings.mFollowTarget && distanceSquared > followRadius)) {
        _follow(mob, targetPosition);
        return;
    }

    mHasPathTarget = false;
    navigation.stop(mob);
    if (mob.isRiding())
        return;

    Vector3f rotation = mob.getRotation();
    rotation.y = LookControl::yawTowards(position, targetPosition);
    mob.setRotation(rotation);
}

// the game repaths to the actor every tick; a new path only when the target changes block keeps the cost down
void RangedAttackGoal::_follow(MobActor &mob, const Vector3f &targetPosition) {
    const int32_t blockX = (int32_t) std::floor(targetPosition.x);
    const int32_t blockY = (int32_t) std::floor(targetPosition.y);
    const int32_t blockZ = (int32_t) std::floor(targetPosition.z);
    if (mHasPathTarget && !mob.getNavigation().isDone() && blockX == mPathTargetX && blockY == mPathTargetY
        && blockZ == mPathTargetZ)
        return;

    mob.getNavigation().moveTo(targetPosition, mSettings.mSpeed);
    mPathTargetX = blockX;
    mPathTargetY = blockY;
    mPathTargetZ = blockZ;
    mHasPathTarget = true;
}

// charge_shoot_trigger mode (ghast, llama): the goal counts the charge, charge_charged_trigger raises the flag
void RangedAttackGoal::_tickCharge(ServerNetworkHandler &owner, MobActor &mob, bool canSee) {
    if (canSee) {
        ++mChargeTime;
        if (mChargeTime >= mSettings.mChargeShootTriggerTicks) {
            mBurstRemaining = mSettings.mBurstShots;
            mBurstCooldown = 0;
            mChargeTime -= 2 * mSettings.mChargeShootTriggerTicks;
        }
    } else {
        --mChargeTime;
    }

    setFlag(owner, mob, ActorFlag::Charged, mChargeTime > mSettings.mChargeChargedTriggerTicks);
    if (mChargeTime == mSettings.mChargeChargedTriggerTicks)
        owner.playLevelSound(owner.getLevelFor(mob), CHARGED_SOUND, mob.getPosition(), mob.getIdentifier());
}

void RangedAttackGoal::_tickBurst(ServerNetworkHandler &owner, MobActor &mob, Actor &target, float distanceSquared,
                                  bool canSee) {
    const int32_t previousCooldown = mBurstCooldown--;
    if (!canSee || previousCooldown >= 2)
        return;

    const float radius = mSettings.mAttackRadius;
    const float radiusMin = mSettings.mAttackRadiusMin;
    if (distanceSquared > radius * radius || distanceSquared < radiusMin * radiusMin || !_inFieldOfView(mob, target))
        return;

    MobShooter::shootAt(owner, mob, target);
    if (mSettings.mChargeShootTriggerTicks <= 0) {
        mAttackTime = _intervalFor(distanceSquared);
        if (mSettings.mSwing)
            owner.broadcastActorEvent(mob, EntityEventType::ArmSwing);
    }

    const int32_t previousBurst = mBurstRemaining--;
    if (previousBurst >= 2) {
        mBurstCooldown = mSettings.mBurstIntervalTicks;
        return;
    }

    // last shot of a loaded crossbow empties it: sub_14D42A9E0
    if (mHeldCharged) {
        mHeldCharged = false;
        CrossbowItem::unloadHeld(owner, mob);
        setFlag(owner, mob, ActorFlag::Charged, false);
    }
}

// attack_interval lerped by target distance over the attack radius, floored
int32_t RangedAttackGoal::_intervalFor(float distanceSquared) const {
    const float radius = mSettings.mAttackRadius;
    const float scale = radius > 0.0f ? std::sqrt(distanceSquared) / radius : 0.0f;
    const float interval = (float) mSettings.mIntervalMinTicks
                           + (float) (mSettings.mIntervalMaxTicks - mSettings.mIntervalMinTicks) * scale;
    return (int32_t) std::floor(interval);
}

// angle between the head's view and the target, against ranged_fov: sub_14333B8F0
bool RangedAttackGoal::_inFieldOfView(const MobActor &mob, const Actor &target) const {
    const Vector3f position = mob.getPosition();
    const Vector3f eye(position.x, position.y + mob.getSize().mHeight * EYE_HEIGHT_RATIO, position.z);
    const Vector3f targetPosition = target.getPosition();
    const float dx = targetPosition.x - eye.x;
    const float dy = targetPosition.y - eye.y;
    const float dz = targetPosition.z - eye.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 0.0001f)
        return mSettings.mRangedFov > 0.0f;

    const Vector3f rotation = mob.getRotation();
    const float pitch = rotation.x * DEGREES_TO_RADIANS;
    const float yaw = rotation.z * DEGREES_TO_RADIANS;
    const float viewX = -std::sin(yaw) * std::cos(pitch);
    const float viewY = -std::sin(pitch);
    const float viewZ = std::cos(yaw) * std::cos(pitch);
    const float dot = std::clamp((viewX * dx + viewY * dy + viewZ * dz) / length, -1.0f, 1.0f);
    return mSettings.mRangedFov > std::acos(dot) * RADIANS_TO_DEGREES;
}
