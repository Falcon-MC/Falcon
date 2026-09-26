#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class MoveToWaterGoal : public Goal {
public:
    MoveToWaterGoal(float speed, int32_t searchRange, int32_t searchHeight, float goalRadius, bool towardsWater);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _isInWater(ServerNetworkHandler &owner, const MobActor &mob) const;

    bool _findTarget(ServerNetworkHandler &owner, const MobActor &mob, Vector3f &target) const;

    float mSpeed;
    int32_t mSearchRange;
    int32_t mSearchHeight;
    float mGoalRadius;
    bool mTowardsWater;
    Vector3f mTarget;
};
