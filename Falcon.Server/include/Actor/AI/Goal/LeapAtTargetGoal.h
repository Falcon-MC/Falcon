#pragma once

#include "Actor/AI/Goal/Goal.h"

class LeapAtTargetGoal : public Goal {
public:
    LeapAtTargetGoal(float height, bool mustBeOnGround);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    float mHeight;
    bool mMustBeOnGround;
};
