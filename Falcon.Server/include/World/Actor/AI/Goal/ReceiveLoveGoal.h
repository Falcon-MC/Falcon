#pragma once

#include "World/Actor/AI/Goal/Goal.h"

// minecraft:behavior.receive_love: the villager a make_love goal picked walks up to the one that picked it
class ReceiveLoveGoal final : public Goal {
public:
    ReceiveLoveGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;
};
