#include "Actor/AI/Goal/UseKineticWeaponGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/RideSystem.h"
#include "Actor/ServerPlayer.h"
#include "Item/Items/SpearItem.h"
#include "Item/VanillaItems.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace {
    const char *const DEATH_MESSAGE = "death.attack.mob";
    const float BASE_REACH = 5.0f;
    const float CHARGE_SPEED_MULTIPLIER = 1.5f;
    const int32_t MAX_CHARGE_TICKS = 60;
    const int32_t COOLDOWN_TICKS = 20;
    const int32_t REPATH_INTERVAL = 10;

    std::mt19937 &kineticRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float horizontalDistance(const Vector3f &left, const Vector3f &right) {
        const float dx = left.x - right.x;
        const float dz = left.z - right.z;
        return std::sqrt(dx * dx + dz * dz);
    }
}

UseKineticWeaponGoal::UseKineticWeaponGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

const SpearItem *UseKineticWeaponGoal::_heldSpear(const MobActor &mob) {
    const ItemStack &held = mob.getEquipment().getSlot(MobEquipment::MAINHAND);
    return held.isAir() ? nullptr
                        : dynamic_cast<const SpearItem *>(VanillaItems::fromIdentifier(held.mDefinition->getIdentifier()));
}

MobActor &UseKineticWeaponGoal::_navigator(ServerNetworkHandler &owner, MobActor &mob) const {
    if (!mSettings.mHijackMountNavigation || !mob.isRiding())
        return mob;

    MobActor *vehicle = dynamic_cast<MobActor *>(RideSystem::resolve(owner, mob.getVehicleId()));
    return vehicle == nullptr ? mob : *vehicle;
}

void UseKineticWeaponGoal::_moveTo(ServerNetworkHandler &owner, MobActor &mob, const Vector3f &target,
                                   float speed) const {
    MobActor &navigator = _navigator(owner, mob);
    navigator.getNavigation().moveTo(target, speed);
}

void UseKineticWeaponGoal::_setCharging(ServerNetworkHandler &owner, MobActor &mob, bool charging) const {
    if (mob.getFlags().get(ActorFlag::UsingItem) == charging)
        return;

    mob.getFlags().set(ActorFlag::UsingItem, charging);
    owner.syncActorFlags(mob);
}

float UseKineticWeaponGoal::_roll(float minimum, float maximum) const {
    return maximum > minimum ? std::uniform_real_distribution<float>(minimum, maximum)(kineticRandom()) : minimum;
}

Vector3f UseKineticWeaponGoal::_awayFrom(const MobActor &mob, const Actor &target, float distance) const {
    const Vector3f position = mob.getPosition();
    const Vector3f targetPosition = target.getPosition();
    float dx = position.x - targetPosition.x;
    float dz = position.z - targetPosition.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length < 0.001f) {
        dx = 1.0f;
        dz = 0.0f;
    } else {
        dx /= length;
        dz /= length;
    }
    return Vector3f(targetPosition.x + dx * distance, position.y, targetPosition.z + dz * distance);
}

bool UseKineticWeaponGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return _heldSpear(mob) != nullptr && mob.getTarget(owner) != nullptr;
}

void UseKineticWeaponGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mPhase = Phase::Approach;
    mLastPosition = mob.getPosition();
    mRepathTicks = 0;
    mPhaseTicks = 0;
}

void UseKineticWeaponGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    _setCharging(owner, mob, false);
    _navigator(owner, mob).getNavigation().stop(_navigator(owner, mob));
}

void UseKineticWeaponGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    const SpearItem *spear = _heldSpear(mob);
    if (target == nullptr || spear == nullptr)
        return;

    mob.getLookControl().setLookAt(target->getPosition());
    const MobActor &navigator = _navigator(owner, mob);
    const float speed = horizontalDistance(navigator.getPosition(), mLastPosition);
    mLastPosition = navigator.getPosition();
    const float distance = horizontalDistance(mob.getPosition(), target->getPosition());
    ++mPhaseTicks;

    switch (mPhase) {
        case Phase::Approach:
            if (distance <= mSettings.mApproachDistance) {
                mPhase = Phase::Charge;
                mPhaseTicks = 0;
                _setCharging(owner, mob, true);
                return;
            }
            if (--mRepathTicks <= 0) {
                mRepathTicks = REPATH_INTERVAL;
                _moveTo(owner, mob, target->getPosition(), mSettings.mSpeed);
            }
            return;

        case Phase::Charge: {
            _moveTo(owner, mob, target->getPosition(), mSettings.mSpeed * CHARGE_SPEED_MULTIPLIER);
            const float reach = BASE_REACH * mSettings.mReachMultiplier;
            const float minimumSpeed = navigator.getMovementSpeed() * mSettings.mMinSpeedMultiplier;
            if (distance <= reach && speed >= minimumSpeed) {
                _strike(owner, mob, *target, *spear, speed);
            } else if (mPhaseTicks < MAX_CHARGE_TICKS) {
                return;
            }

            _setCharging(owner, mob, false);
            mPhase = Phase::Reposition;
            mPhaseTicks = 0;
            mPhaseDistance = _roll(mSettings.mMinRepositionDistance, mSettings.mMaxRepositionDistance);
            _moveTo(owner, mob, _awayFrom(mob, *target, mPhaseDistance), mSettings.mSpeed);
            return;
        }

        case Phase::Reposition:
            if (distance < mPhaseDistance && !_navigator(owner, mob).getNavigation().isDone())
                return;

            mPhase = Phase::Cooldown;
            mPhaseTicks = 0;
            mPhaseDistance = _roll(mSettings.mMinCooldownDistance, mSettings.mMaxCooldownDistance);
            _moveTo(owner, mob, _awayFrom(mob, *target, mPhaseDistance), mSettings.mSpeed);
            return;

        case Phase::Cooldown:
            if (mPhaseTicks < COOLDOWN_TICKS)
                return;

            mPhase = Phase::Approach;
            mPhaseTicks = 0;
            mRepathTicks = 0;
            return;
    }
}

void UseKineticWeaponGoal::_strike(ServerNetworkHandler &owner, MobActor &mob, Actor &target, const SpearItem &spear,
                                   float speed) {
    const float damage = spear.getKineticDamage(speed);
    const float healthBefore = target.getHealth();
    if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&target)) {
        owner.hurt(*player, damage, ActorDamageSource::attack(DEATH_MESSAGE, player->getName(), mob, mob.getName(),
                                                              mob.getPosition()));
    } else if (ServerActor *actor = dynamic_cast<ServerActor *>(&target)) {
        owner.damageActor(*actor, damage, &mob);
    }

    owner.broadcastActorEvent(mob, EntityEventType::ArmSwing);
    owner.playLevelSound(owner.getLevelFor(mob), "item." + spear.getTierName() + ".attack_hit", mob.getPosition(),
                         mob.getIdentifier());
    if (target.getHealth() < healthBefore)
        mob.getAnger().onAttack(owner, mob, target);
}
