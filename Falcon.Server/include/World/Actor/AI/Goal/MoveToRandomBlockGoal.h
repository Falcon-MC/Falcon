#pragma once

#include "Core/Math/Vector3f.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <random>

class Level;

// a patrol captain's march to a far random block, in steps, sidestepping when stuck
class MoveToRandomBlockGoal : public Goal {
public:
    MoveToRandomBlockGoal(float speed, float withinRadius, int32_t blockDistance);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _isValid(Level &level) const;

    float mSpeed;
    float mWithinRadius;
    int32_t mBlockDistance;
    std::mt19937 mRandom;
    bool mHasTarget = false;
    Vector3f mTarget;
    Vector3f mLastPosition;
    int32_t mStuckTicks = 0;
    int32_t mStuckCount = 0;
    int32_t mCooldown = 0;
};
