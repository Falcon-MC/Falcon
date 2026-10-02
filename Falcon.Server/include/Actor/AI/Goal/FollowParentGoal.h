#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class FollowParentGoal : public Goal {
public:
    explicit FollowParentGoal(float speed);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    MobActor *_findParent(ServerNetworkHandler &owner, const MobActor &mob) const;

    float mSpeed;
    int64_t mParentId = 0;
    int32_t mTicksUntilRepath = 0;
};
