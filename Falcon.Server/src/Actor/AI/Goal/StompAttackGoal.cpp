#include "Actor/AI/Goal/StompAttackGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"

StompAttackGoal::StompAttackGoal(float speed, float maxRange, int32_t coolDown, float stompRangeSquared,
                                 float noDamageRangeSquared)
        : MeleeAttackGoal(speed, maxRange, coolDown, stompRangeSquared), mNoDamageRangeSquared(noDamageRangeSquared) {
}

void StompAttackGoal::_tryAttack(ServerNetworkHandler &owner, MobActor &mob, Actor &target) {
    if (mTicksSinceAttack <= mCoolDown)
        return;

    const float distance = mob.distanceSquaredTo(target);
    if (distance <= mAttackRangeSquared) {
        _attack(owner, mob, target);
        return;
    }

    if (distance > mNoDamageRangeSquared)
        return;

    owner.broadcastActorEvent(mob, EntityEventType::ArmSwing);
    mTicksSinceAttack = 0;
}
