#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class ServerPlayer;

class LookAtPlayerGoal : public Goal {
public:
    LookAtPlayerGoal(float range, int32_t probability, int32_t total, int32_t duration, int32_t checkInterval);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    ServerPlayer *_findNearestPlayer(ServerNetworkHandler &owner, const MobActor &mob) const;

    float mRange;
    int32_t mProbability;
    int32_t mTotal;
    int32_t mDuration;
    int32_t mCheckInterval;
    int32_t mTicksUntilCheck = 0;
    int32_t mTicksRunning = 0;
};
