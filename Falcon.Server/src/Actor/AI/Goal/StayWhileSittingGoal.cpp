#include "Actor/AI/Goal/StayWhileSittingGoal.h"

#include "Actor/Mob/MobActor.h"

StayWhileSittingGoal::StayWhileSittingGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Jump);
}

bool StayWhileSittingGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mob.isSitting();
}

void StayWhileSittingGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}
