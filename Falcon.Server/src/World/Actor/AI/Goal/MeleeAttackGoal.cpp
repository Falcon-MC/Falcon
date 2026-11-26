#include "World/Actor/AI/Goal/MeleeAttackGoal.h"

#include "World/Actor/AI/Goal/MobAttack.h"
#include "World/Actor/Definition/KnockbackRules.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"

#include <cmath>

namespace {
    const float ATTACK_KNOCKBACK = 0.3f;
    const char *DEATH_MESSAGE = "death.attack.mob";
    // the game's min_path_time and max_path_time defaults, 0.2 and 0.55 seconds
    const int32_t MIN_REPATH_TICKS = 4;
    const int32_t MAX_REPATH_TICKS = 11;
}

MeleeAttackGoal::MeleeAttackGoal(float speed, float maxRange, int32_t coolDown, float attackRangeSquared)
        : mCoolDown(coolDown), mAttackRangeSquared(attackRangeSquared), mSpeed(speed),
          mMaxRangeSquared(maxRange * maxRange) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool MeleeAttackGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return target != nullptr && mob.distanceSquaredTo(*target) <= mMaxRangeSquared;
}

void MeleeAttackGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mTicksSinceAttack = 0;
    mTicksUntilRepath = 0;
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

    Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;

    const Vector3f targetPosition = target->getPosition();
    const int32_t blockX = (int32_t) std::floor(targetPosition.x);
    const int32_t blockY = (int32_t) std::floor(targetPosition.y);
    const int32_t blockZ = (int32_t) std::floor(targetPosition.z);
    // the game also repaths on a timer: a mob knocked back once its path ran out, by a shield say, walks back to
    // a target still on the same block
    const bool moved = !mHasLastTarget || blockX != mLastTargetX || blockY != mLastTargetY || blockZ != mLastTargetZ;
    if (mTicksUntilRepath > 0)
        mTicksUntilRepath--;
    if (moved || (mTicksUntilRepath == 0 && mob.getNavigation().isDone())) {
        mob.getNavigation().moveTo(targetPosition, mSpeed);
        mTicksUntilRepath = MobActor::randomRange(MIN_REPATH_TICKS, MAX_REPATH_TICKS);
        mLastTargetX = blockX;
        mLastTargetY = blockY;
        mLastTargetZ = blockZ;
        mHasLastTarget = true;
    }

    mob.getLookControl().setLookAt(targetPosition);
    _tryAttack(owner, mob, *target);
}

void MeleeAttackGoal::_tryAttack(ServerNetworkHandler &owner, MobActor &mob, Actor &target) {
    if (mTicksSinceAttack > mCoolDown && mob.distanceSquaredTo(target) <= mAttackRangeSquared)
        _attack(owner, mob, target);
}

void MeleeAttackGoal::_attack(ServerNetworkHandler &owner, MobActor &mob, Actor &target) {
    const float damage = mob.getAttackDamage();
    if (damage <= 0.0f)
        return;

    // a hit that lands pushes even when armor or resistance took all of it
    bool landed = false;
    if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&target)) {
        const ActorDamageSource source = ActorDamageSource::attack(DEATH_MESSAGE, player->getName(), mob, mob.getName(),
                                                                   mob.getPosition());
        const DamageResult result = owner.hurt(*player, damage, source);
        if (result == DamageResult::Blocked) {
            owner.broadcastActorEvent(mob, EntityEventType::ArmSwing);
            mTicksSinceAttack = 0;
            mob.onAttackBlocked(owner, *player);
            return;
        }
        landed = result == DamageResult::Dealt;
    } else if (ServerActor *actor = dynamic_cast<ServerActor *>(&target)) {
        landed = owner.damageActor(*actor, damage, &mob);
    }

    if (!landed)
        return;

    mob.getAnger().onAttack(owner, mob, target);
    MobAttack::applyAttackEffect(owner, mob, target);

    const Vector3f mobPosition = mob.getPosition();
    const Vector3f targetPosition = target.getPosition();
    KnockbackPreset preset;
    if (KnockbackRules::select(owner, mob, target, preset))
        owner.knockBack(target, targetPosition.x - mobPosition.x, targetPosition.z - mobPosition.z, preset);
    else
        owner.knockBack(target, targetPosition.x - mobPosition.x, targetPosition.z - mobPosition.z, ATTACK_KNOCKBACK);
    owner.broadcastActorEvent(mob, EntityEventType::ArmSwing);
    mTicksSinceAttack = 0;
}
