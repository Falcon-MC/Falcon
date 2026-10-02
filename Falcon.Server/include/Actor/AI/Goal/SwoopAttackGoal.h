#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class SwoopAttackGoal : public Goal {
public:
    SwoopAttackGoal(float speed, float damageReach, int32_t minDelayTicks, int32_t maxDelayTicks);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    void _scheduleNext(ServerNetworkHandler &owner);

    float mSpeed;
    float mDamageReach;
    int32_t mMinDelayTicks;
    int32_t mMaxDelayTicks;
    int64_t mNextSwoopTick = -1;
    uint32_t mHurtCount = 0;
    bool mDone = false;
};
