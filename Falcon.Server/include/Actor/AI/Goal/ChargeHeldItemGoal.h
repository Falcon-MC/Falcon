#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class MobActor;

class ChargeHeldItemGoal : public Goal {
public:
    ChargeHeldItemGoal();

    static bool holdsCrossbow(const MobActor &mob);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    int32_t mTicks = 0;
};
