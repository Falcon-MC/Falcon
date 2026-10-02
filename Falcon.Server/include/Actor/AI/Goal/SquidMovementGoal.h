#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class SquidMovementGoal : public Goal {
public:
    SquidMovementGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    void _pickDirection();

    bool _tickFlee(ServerNetworkHandler &owner, MobActor &mob);

    void _tickOutOfWater(MobActor &mob);

    Vector3f mDirection;
    bool mHasDirection = false;
    uint32_t mFleeHurtCount = 0;
    int32_t mFleeTicks = 0;
};
