#include "Actor/AI/Goal/HurtByTargetGoal.h"

#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <utility>

HurtByTargetGoal::HurtByTargetGoal(std::shared_ptr<json::Value> filters) : mFilters(std::move(filters)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Target);
}

bool HurtByTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getHurtCount() == mHandledHurtCount || mob.getLastHurtBy() == 0)
        return false;

    const Actor *attacker = MobActor::findActor(owner, mob.getLastHurtBy());
    if (attacker == nullptr || !mob.canTarget(*attacker)
        || (mFilters != nullptr && !EntityFilter::test(*mFilters, owner, mob, attacker))) {
        mHandledHurtCount = mob.getHurtCount();
        return false;
    }

    return true;
}

bool HurtByTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mob.getTarget(owner) != nullptr;
}

void HurtByTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mHandledHurtCount = mob.getHurtCount();
    mob.setTarget(owner, mob.getLastHurtBy());
}

void HurtByTargetGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.clearTarget();
}
