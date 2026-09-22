#pragma once

#include "Actor/AI/Goal/Goal.h"

class FloatGoal : public Goal {
public:
    FloatGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool mEyesWereInWater = false;
};
