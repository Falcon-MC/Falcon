#include "Actor/AI/Goal/SitGoal.h"

#include "Actor/Mob/MobActor.h"

SitGoal::SitGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Jump);
}

bool SitGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mob.isSitting();
}

void SitGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}
