#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

class Level;

class RiseToLiquidLevelGoal : public Goal {
public:
    RiseToLiquidLevelGoal(float liquidYOffset, float riseDelta, float sinkDelta);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

    // the feet height the mob settles at, from its feet up through the liquid column at its centre
    static float targetHeight(Level &level, const Vector3f &feet, float liquidYOffset);

private:
    float mLiquidYOffset;
    float mRiseDelta;
    float mSinkDelta;
};
