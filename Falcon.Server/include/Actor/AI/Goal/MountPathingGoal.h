#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class Actor;

class MountPathingGoal : public Goal {
public:
    MountPathingGoal(float speed, float targetDistance, bool trackTarget);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    static Actor *_riderTarget(ServerNetworkHandler &owner, const MobActor &mob);

    float mSpeed;
    float mTargetDistance;
    bool mTrackTarget;
    int32_t mRepathTicks = 0;
};
