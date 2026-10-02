#include "Actor/AI/Goal/GuardianAttackGoal.h"

#include "Actor/AI/Goal/MobAttack.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"

namespace {
    const float MAX_RANGE_SQUARED = 15.0f * 15.0f;
    const int32_t COOL_DOWN_TICKS = 60;
    const int32_t ATTACK_DELAY_TICKS = 40;
    const float MAGIC_DAMAGE = 1.0f;
    const char *const MAGIC_DEATH_MESSAGE = "death.attack.indirectMagic";
    const char *const WARNING_SOUND = "mob.warning";
    const EntityEventType GUARDIAN_ATTACK = (EntityEventType) 28;

    void hurtWithMagic(ServerNetworkHandler &owner, MobActor &mob, Actor &target) {
        const ActorDamageSource source = ActorDamageSource::attack(MAGIC_DEATH_MESSAGE, target.getName(), mob,
                                                                   mob.getName(), mob.getPosition())
                .withoutArmor()
                .withoutCooldown();
        if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&target))
            owner.hurt(*player, MAGIC_DAMAGE, source);
        else if (ServerActor *actor = dynamic_cast<ServerActor *>(&target))
            owner.damageActor(*actor, MAGIC_DAMAGE, source);
    }
}

GuardianAttackGoal::GuardianAttackGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool GuardianAttackGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return target != nullptr && target->isAlive() && mob.distanceSquaredTo(*target) <= MAX_RANGE_SQUARED;
}

void GuardianAttackGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mCoolDownTicks = 0;
    mChargeTicks = 0;
    mCharging = false;
    mob.getLookControl().setPitchEnabled(true);
}

void GuardianAttackGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    if (mCharging)
        _setBeamTarget(owner, mob, 0);

    mCharging = false;
    mob.getLookControl().clear();
    mob.getLookControl().setPitchEnabled(false);
}

void GuardianAttackGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;

    mob.getNavigation().stop(mob);
    mob.getLookControl().setLookAt(target->getPosition());

    if (!mCharging) {
        if (++mCoolDownTicks <= COOL_DOWN_TICKS)
            return;

        mCharging = true;
        mChargeTicks = 0;
        _setBeamTarget(owner, mob, target->getUniqueId());
        owner.playLevelSound(owner.getLevelFor(mob), WARNING_SOUND, mob.getPosition(), mob.getIdentifier());
        return;
    }

    if (++mChargeTicks <= ATTACK_DELAY_TICKS)
        return;

    mCharging = false;
    mCoolDownTicks = 0;
    _setBeamTarget(owner, mob, 0);
    owner.broadcastActorEvent(mob, GUARDIAN_ATTACK);

    const Difficulty difficulty = owner.getProperties().getDifficulty();
    MobAttack::hit(owner, mob, *target, mob.getAttackDamage(difficulty));
    if (difficulty == Difficulty::Normal || difficulty == Difficulty::Hard)
        hurtWithMagic(owner, mob, *target);
}

void GuardianAttackGoal::_setBeamTarget(ServerNetworkHandler &owner, MobActor &mob, int64_t uniqueId) {
    EntityDataEntry entry;
    entry.mId = ActorFlags::TARGET_DATA_ID;
    entry.mFormat = EntityDataFormat::Long;
    entry.mLongValue = uniqueId;

    EntityDataMap metadata;
    metadata.mEntries.push_back(entry);
    owner.sendActorMetadata(mob, metadata);
}
