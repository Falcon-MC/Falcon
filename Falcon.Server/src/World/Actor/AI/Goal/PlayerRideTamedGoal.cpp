#include "World/Actor/AI/Goal/PlayerRideTamedGoal.h"

#include "World/Actor/ActorFlags.h"
#include "World/Actor/Mob/MobActor.h"

PlayerRideTamedGoal::PlayerRideTamedGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

// any rider counts, not only a player: the game looks at the tamed flag and the passengers alone
bool PlayerRideTamedGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return !mob.getPassengers().empty() && mob.getFlags().get(ActorFlag::Tamed);
}
