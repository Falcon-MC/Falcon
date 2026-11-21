#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

// minecraft:behavior.move_towards_target: walks to a random ground point on the way to the target
class MoveTowardsTargetGoal : public Goal {
public:
    MoveTowardsTargetGoal(float speed, float withinRadius);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _isTargetInRange(ServerNetworkHandler &owner, const MobActor &mob, bool strict) const;

    float mSpeed;
    float mWithinRadius;
    Vector3f mWanted;
};
