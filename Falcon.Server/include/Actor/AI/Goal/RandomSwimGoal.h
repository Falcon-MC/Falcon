#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class RandomSwimGoal : public Goal {
public:
    RandomSwimGoal(float speed, int32_t horizontalRange, int32_t verticalRange, int32_t interval);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _findWater(ServerNetworkHandler &owner, const MobActor &mob, Vector3f &target) const;

    float mSpeed;
    int32_t mHorizontalRange;
    int32_t mVerticalRange;
    int32_t mInterval;
    Vector3f mTarget;
};
