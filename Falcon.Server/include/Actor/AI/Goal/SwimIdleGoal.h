#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class SwimIdleGoal : public Goal {
public:
    SwimIdleGoal(int32_t idleTicks, float successRate);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    int32_t mIdleTicks;
    float mSuccessRate;
    int32_t mRemaining = 0;
};
