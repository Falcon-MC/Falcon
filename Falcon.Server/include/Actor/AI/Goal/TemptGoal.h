#pragma once

#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class TemptGoal : public Goal {
public:
    TemptGoal(float speed, float range, BehaviorItems items, float stopDistance);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    float mSpeed;
    float mRange;
    BehaviorItems mItems;
    float mStopDistanceSquared;
    int32_t mTicksUntilRepath = 0;
};
