#pragma once

#include "Actor/AI/Goal/RandomStrollGoal.h"

class PanicGoal : public RandomStrollGoal {
public:
    PanicGoal(float speed, int32_t range, int32_t interval, int32_t duration, bool avoidWater, int32_t maxRetries);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool shouldPickTarget(MobActor &mob) const override;

private:
    int32_t mDuration;
};
