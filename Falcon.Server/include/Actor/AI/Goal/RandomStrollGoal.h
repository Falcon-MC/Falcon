#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class Level;

class RandomStrollGoal : public Goal {
public:
    RandomStrollGoal(float speed, int32_t range, int32_t interval, bool avoidWater, int32_t maxRetries);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Vector3f _randomTarget(const MobActor &mob) const;

    bool _isAboveWater(Level &level, const Vector3f &target) const;

    float mSpeed;
    int32_t mRange;
    int32_t mInterval;
    bool mAvoidWater;
    int32_t mMaxRetries;
    int32_t mTicksSinceTarget = 0;
};
