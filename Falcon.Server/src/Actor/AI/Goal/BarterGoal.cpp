#include "Actor/AI/Goal/BarterGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>

namespace {
    const char *const BARTER_COMPONENT = "minecraft:barter";
    const float TICKS_PER_SECOND = 20.0f;
}

bool BarterGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *barter = mob.getComponent(BARTER_COMPONENT);
    if (barter == nullptr || !mob.getAdmiration().isReadyToBarter())
        return false;

    const json::Value *cooldown = barter->get("cooldown_after_being_attacked");
    const int64_t cooldownTicks = cooldown == nullptr ? 0
                                                      : (int64_t) std::lround(cooldown->number(0.0) * TICKS_PER_SECOND);
    return owner.getCurrentTick() - mob.getLastHurtTick() >= cooldownTicks;
}

bool BarterGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return false;
}

void BarterGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mob.getAdmiration().barter(owner, mob);
}
