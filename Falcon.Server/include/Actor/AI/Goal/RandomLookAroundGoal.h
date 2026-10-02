#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class RandomLookAroundGoal : public Goal {
public:
    RandomLookAroundGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    float mOffsetX = 0.0f;
    float mOffsetZ = 0.0f;
    int32_t mLookTime = 0;
};
