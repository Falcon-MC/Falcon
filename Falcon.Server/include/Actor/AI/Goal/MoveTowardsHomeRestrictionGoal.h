#pragma once

#include "Actor/AI/Goal/Goal.h"

class MoveTowardsHomeRestrictionGoal : public Goal {
public:
    MoveTowardsHomeRestrictionGoal(float speed, float radius);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _isOutside(const MobActor &mob) const;

    float mSpeed;
    float mRadius;
};
