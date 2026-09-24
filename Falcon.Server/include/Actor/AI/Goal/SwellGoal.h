#pragma once

#include "Actor/AI/Goal/Goal.h"

class SwellGoal : public Goal {
public:
    SwellGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;
};
