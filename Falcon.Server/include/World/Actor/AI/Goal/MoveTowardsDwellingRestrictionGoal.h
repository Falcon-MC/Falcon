#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

// minecraft:behavior.move_towards_dwelling_restriction: a dweller outside its village walks back to a random
// column inside the village box
class MoveTowardsDwellingRestrictionGoal : public Goal {
public:
    explicit MoveTowardsDwellingRestrictionGoal(float speed);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    float mSpeed;
    int32_t mIntervalLeft = 0;
    int32_t mTicksMoving = 0;
    bool mReachedInside = false;
    Vector3f mTarget;
};
