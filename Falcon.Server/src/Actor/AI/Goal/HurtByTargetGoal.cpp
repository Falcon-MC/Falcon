#include "Actor/AI/Goal/HurtByTargetGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

HurtByTargetGoal::HurtByTargetGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Target);
}

bool HurtByTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getHurtCount() == mHandledHurtCount || mob.getLastHurtBy() == 0)
        return false;

    const ServerPlayer *attacker = MobActor::findPlayer(owner, mob.getLastHurtBy());
    if (attacker == nullptr || !mob.canTarget(*attacker)) {
        mHandledHurtCount = mob.getHurtCount();
        return false;
    }
    return true;
}

bool HurtByTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mob.getTarget(owner) != nullptr;
}

void HurtByTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mHandledHurtCount = mob.getHurtCount();
    mob.setTarget(mob.getLastHurtBy());
}

void HurtByTargetGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.clearTarget();
}
