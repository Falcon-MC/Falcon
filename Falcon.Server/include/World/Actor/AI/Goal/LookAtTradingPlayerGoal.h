#pragma once

#include "World/Actor/AI/Goal/Goal.h"

// minecraft:behavior.look_at_trading_player: a trader watches the player trading with it
class LookAtTradingPlayerGoal final : public Goal {
public:
    LookAtTradingPlayerGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;
};
