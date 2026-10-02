#include "Actor/AI/Goal/RangedAttackGoal.h"

#include "Actor/AI/Goal/ChargeHeldItemGoal.h"
#include "Actor/ActorClassRegistry.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Item/Items/RangedWeaponHelpers.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace {
    const float PROJECTILE_SPEED = 1.6f;
    const float ARC_PER_BLOCK = 0.2f;
    const float EYE_RATIO = 0.85f;
    const float TARGET_BODY_RATIO = 0.33f;
    const float PLAYER_HEIGHT = 1.8f;
    const int32_t REPATH_INTERVAL = 10;
    const int32_t PULL_BOW_TICKS = 20;

    std::mt19937 &rangedRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

RangedAttackGoal::RangedAttackGoal(float speed, float range, int32_t minInterval, int32_t maxInterval,
                                   std::string projectile)
        : mSpeed(speed), mRange(range), mMinInterval(minInterval), mMaxInterval(std::max(minInterval, maxInterval)),
          mProjectile(std::move(projectile)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

int32_t RangedAttackGoal::_nextInterval() const {
    return std::uniform_int_distribution<int32_t>(mMinInterval, mMaxInterval)(rangedRandom());
}

bool RangedAttackGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return !mProjectile.empty() && ActorClassRegistry::getPrototype(mProjectile) != nullptr
           && mob.getTarget(owner) != nullptr;
}

void RangedAttackGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mCooldown = _nextInterval();
    mTicksUntilRepath = 0;
    mDrawTicks = 0;
}

void RangedAttackGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    _stopBowAnimation(owner, mob);
    mDrawTicks = 0;
    mob.getNavigation().stop(mob);
    mob.getLookControl().clear();
}

void RangedAttackGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr) {
        if (mDrawTicks > 0) {
            _stopBowAnimation(owner, mob);
            mDrawTicks = 0;
        }
        return;
    }

    const Vector3f targetPosition = target->getPosition();
    mob.getLookControl().setLookAt(targetPosition);

    const bool inRange = mob.distanceSquaredTo(*target) <= mRange * mRange;
    if (inRange) {
        mob.getNavigation().stop(mob);
    } else if (--mTicksUntilRepath <= 0) {
        mTicksUntilRepath = REPATH_INTERVAL;
        mob.getNavigation().moveTo(targetPosition, mSpeed);
    }

    const bool crossbow = mob.getComponent("minecraft:behavior.charge_held_item") != nullptr
                          && ChargeHeldItemGoal::holdsCrossbow(mob);

    if (mDrawTicks > 0) {
        if (++mDrawTicks > PULL_BOW_TICKS) {
            _shoot(owner, mob, *target, false);
            _stopBowAnimation(owner, mob);
            mDrawTicks = 0;
            mCooldown = _nextInterval();
        }
        return;
    }

    if (--mCooldown > 0 || !inRange)
        return;

    if (crossbow && !mob.getFlags().get(ActorFlag::Charged))
        return;

    if (crossbow) {
        mCooldown = _nextInterval();
        _shoot(owner, mob, *target, true);
        return;
    }

    mDrawTicks = 1;
    _playBowAnimation(owner, mob, *target);
}

void RangedAttackGoal::_playBowAnimation(ServerNetworkHandler &owner, MobActor &mob, const Actor &target) {
    _setTargetId(owner, mob, target.getUniqueId());
    if (!mob.getFlags().get(ActorFlag::FacingTargetToRangeAttack)) {
        mob.getFlags().set(ActorFlag::FacingTargetToRangeAttack, true);
        owner.syncActorFlags(mob);
    }
}

void RangedAttackGoal::_stopBowAnimation(ServerNetworkHandler &owner, MobActor &mob) {
    _setTargetId(owner, mob, 0);
    if (mob.getFlags().get(ActorFlag::FacingTargetToRangeAttack)) {
        mob.getFlags().set(ActorFlag::FacingTargetToRangeAttack, false);
        owner.syncActorFlags(mob);
    }
}

void RangedAttackGoal::_setTargetId(ServerNetworkHandler &owner, MobActor &mob, int64_t uniqueId) {
    EntityDataEntry entry;
    entry.mId = ActorFlags::TARGET_DATA_ID;
    entry.mFormat = EntityDataFormat::Long;
    entry.mLongValue = uniqueId;
    EntityDataMap metadata;
    metadata.mEntries.push_back(entry);
    owner.sendActorMetadata(mob, metadata);
}

void RangedAttackGoal::_shoot(ServerNetworkHandler &owner, MobActor &mob, const Actor &target, bool crossbow) {
    if (crossbow) {
        mob.getFlags().set(ActorFlag::Charged, false);
        owner.syncActorFlags(mob);
    }

    const Vector3f position = mob.getPosition();
    const Vector3f origin(position.x, position.y + mob.getSize().mHeight * EYE_RATIO, position.z);
    const Vector3f targetPosition = target.getPosition();
    const float dx = targetPosition.x - origin.x;
    const float dz = targetPosition.z - origin.z;
    const float horizontal = std::sqrt(dx * dx + dz * dz);
    const float dy = targetPosition.y + PLAYER_HEIGHT * TARGET_BODY_RATIO - origin.y + horizontal * ARC_PER_BLOCK;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 0.0001f)
        return;

    ServerActor *projectile = owner.spawnActor(owner.getLevelFor(mob), mProjectile, origin);
    if (projectile == nullptr)
        return;

    projectile->setProjectile(true);
    projectile->setOwnerUniqueId((int64_t) mob.getRuntimeId());
    projectile->getProjectileData().mBaseDamage = RangedWeaponHelpers::ARROW_BASE_DAMAGE;
    projectile->setMotion(Vector3f(dx / length * PROJECTILE_SPEED, dy / length * PROJECTILE_SPEED,
                                   dz / length * PROJECTILE_SPEED));
    owner.allowProjectileLaunch(*projectile, &mob);
}
