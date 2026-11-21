#pragma once

#include "World/Actor/AI/Goal/Goal.h"

class FloatGoal : public Goal {
public:
    FloatGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

    bool keepsAfloat() const override {
        return true;
    }

private:
    bool mEyesWereInWater = false;
};
