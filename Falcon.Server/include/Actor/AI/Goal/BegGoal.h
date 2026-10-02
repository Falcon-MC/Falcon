#pragma once

#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class BegGoal : public Goal {
public:
    BegGoal(BehaviorItems items, float lookDistance, int32_t minLookTicks, int32_t maxLookTicks);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    static void _setInterested(ServerNetworkHandler &owner, MobActor &mob, bool interested);

    BehaviorItems mItems;
    float mLookDistance;
    int32_t mMinLookTicks;
    int32_t mMaxLookTicks;
    int32_t mLookTicks = 0;
};
