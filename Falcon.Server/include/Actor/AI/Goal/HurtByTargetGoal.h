#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class HurtByTargetGoal : public Goal {
public:
    HurtByTargetGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    uint32_t mHandledHurtCount = 0;
};
