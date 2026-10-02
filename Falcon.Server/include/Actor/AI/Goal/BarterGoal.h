#pragma once

#include "Actor/AI/Goal/Goal.h"

class BarterGoal : public Goal {
public:
    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;
};
