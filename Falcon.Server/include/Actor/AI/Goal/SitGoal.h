#pragma once

#include "Actor/AI/Goal/Goal.h"

class SitGoal : public Goal {
public:
    SitGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;
};
