#include "World/Actor/AI/Goal/LookAtTradingPlayerGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"

LookAtTradingPlayerGoal::LookAtTradingPlayerGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Look);
}

bool LookAtTradingPlayerGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *player = mob.getEconomy().findTradingPlayer(owner);
    return player != nullptr && player->isAlive();
}

void LookAtTradingPlayerGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (const ServerPlayer *player = mob.getEconomy().findTradingPlayer(owner))
        mob.getLookControl().setLookAt(player->getPosition());
}
