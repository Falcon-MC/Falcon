#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class ServerPlayer;

class FollowOwnerGoal : public Goal {
public:
    FollowOwnerGoal(float speed, float startDistance, float stopDistance);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    ServerPlayer *_findOwner(ServerNetworkHandler &owner, const MobActor &mob) const;

    float mSpeed;
    float mStartDistance;
    float mStopDistance;
    int32_t mTicksUntilRepath = 0;
};
