#pragma once

#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class ServerPlayer;

class TemptGoal : public Goal {
public:
    TemptGoal(float speed, float range, BehaviorItems items);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    ServerPlayer *_findTempter(ServerNetworkHandler &owner, const MobActor &mob) const;

    float mSpeed;
    float mRange;
    BehaviorItems mItems;
    int32_t mTicksUntilRepath = 0;
};
