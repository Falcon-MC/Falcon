#include "Actor/AI/Goal/MeleeAttackGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"

#include <cmath>

namespace {
    const float ATTACK_KNOCKBACK = 0.3f;
    const char *DEATH_MESSAGE = "death.attack.mob";
}

MeleeAttackGoal::MeleeAttackGoal(float speed, float maxRange, int32_t coolDown, float attackRangeSquared)
        : mSpeed(speed), mMaxRangeSquared(maxRange * maxRange), mCoolDown(coolDown),
          mAttackRangeSquared(attackRangeSquared) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool MeleeAttackGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *target = mob.getTarget(owner);
    return target != nullptr && mob.distanceSquaredTo(*target) <= mMaxRangeSquared;
}

void MeleeAttackGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mTicksSinceAttack = 0;
    mHasLastTarget = false;
    mob.getLookControl().setPitchEnabled(true);
}

void MeleeAttackGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
    mob.getLookControl().clear();
    mob.getLookControl().setPitchEnabled(false);
}

void MeleeAttackGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    mTicksSinceAttack++;

    ServerPlayer *target = mob.getTarget(owner);
    if (target == nullptr)
        return;

    const Vector3f targetPosition = target->getPosition();
    const int32_t blockX = (int32_t) std::floor(targetPosition.x);
    const int32_t blockY = (int32_t) std::floor(targetPosition.y);
    const int32_t blockZ = (int32_t) std::floor(targetPosition.z);
    if (!mHasLastTarget || blockX != mLastTargetX || blockY != mLastTargetY || blockZ != mLastTargetZ) {
        mob.getNavigation().moveTo(targetPosition, mSpeed);
        mLastTargetX = blockX;
        mLastTargetY = blockY;
        mLastTargetZ = blockZ;
        mHasLastTarget = true;
    }

    mob.getLookControl().setLookAt(targetPosition);

    if (mTicksSinceAttack > mCoolDown && mob.distanceSquaredTo(*target) <= mAttackRangeSquared)
        _attack(owner, mob, *target);
}

void MeleeAttackGoal::_attack(ServerNetworkHandler &owner, MobActor &mob, ServerPlayer &target) {
    const float damage = mob.getAttackDamage(owner.getProperties().getDifficulty());
    if (damage <= 0.0f)
        return;

    if (target.blockWithShield(owner, mob.getPosition(), damage, &mob, false))
        return;

    const float healthBefore = target.getHealth();
    owner.applyDamage(target, damage, DEATH_MESSAGE, {target.getName(), mob.getName()}, true, true, &mob);
    if (target.getHealth() >= healthBefore)
        return;

    const Vector3f mobPosition = mob.getPosition();
    const Vector3f targetPosition = target.getPosition();
    owner.knockBack(target, targetPosition.x - mobPosition.x, targetPosition.z - mobPosition.z, ATTACK_KNOCKBACK);
    owner.broadcastActorEvent(mob, EntityEventType::ArmSwing);
    mTicksSinceAttack = 0;
}
