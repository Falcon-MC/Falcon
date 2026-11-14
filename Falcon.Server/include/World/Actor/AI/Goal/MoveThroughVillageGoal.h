#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

// minecraft:behavior.move_through_village: walks to the closest of ten random ground points open to the sky
class MoveThroughVillageGoal : public Goal {
public:
    MoveThroughVillageGoal(float speed, bool onlyAtNight);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _findTarget(ServerNetworkHandler &owner, const MobActor &mob);

    float mSpeed;
    bool mOnlyAtNight;
    int32_t mIntervalLeft = 0;
    int32_t mTicksMoving = 0;
    int64_t mNextSearchTick = 0;
    Vector3f mTarget;
};
