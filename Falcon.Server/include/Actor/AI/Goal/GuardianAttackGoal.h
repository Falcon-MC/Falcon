#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class GuardianAttackGoal : public Goal {
public:
    GuardianAttackGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    static void _setBeamTarget(ServerNetworkHandler &owner, MobActor &mob, int64_t uniqueId);

    int32_t mCoolDownTicks = 0;
    int32_t mChargeTicks = 0;
    bool mCharging = false;
};
