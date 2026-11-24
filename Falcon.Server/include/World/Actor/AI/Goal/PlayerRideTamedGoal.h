#pragma once

#include "World/Actor/AI/Goal/Goal.h"

// holds the mob's own moving and looking while it carries a rider, once tamed; the rider steers it, or the
// mob stands still under a rider that cannot
class PlayerRideTamedGoal : public Goal {
public:
    PlayerRideTamedGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;
};
