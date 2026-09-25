#pragma once

#include "Actor/AI/Goal/Goal.h"

class StayWhileSittingGoal : public Goal {
public:
    StayWhileSittingGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;
};
