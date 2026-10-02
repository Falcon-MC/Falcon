#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class StompTurtleEggGoal : public Goal {
public:
    StompTurtleEggGoal(float speed, int32_t searchRange, int32_t searchHeight, float goalRadius, int32_t interval);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _isEgg(ServerNetworkHandler &owner, const MobActor &mob, const Vector3i &position) const;

    bool _findEgg(ServerNetworkHandler &owner, const MobActor &mob, Vector3i &egg) const;

    float mSpeed;
    int32_t mSearchRange;
    int32_t mSearchHeight;
    float mGoalRadius;
    int32_t mInterval;
    Vector3i mEgg;
    int32_t mStompTicks = 0;
    int32_t mRepathTicks = 0;
};
