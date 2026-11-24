#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

// minecraft:behavior.explore_outskirts: a villager walks out past the edge of its village, looks around a while
// and comes back
class ExploreOutskirtsGoal final : public Goal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        float mExploreDistance = 0.0f;
        float mMinDistanceFromTarget = 0.0f;
        int32_t mMaxWaitTicks = 0;
    };

    explicit ExploreOutskirtsGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Settings mSettings;
    Vector3f mTarget;
    // where the walk out began: the villager returns there after looking around
    Vector3f mHome;
    int32_t mWaitTicks = 0;
    bool mArrived = false;
    bool mReturning = false;
};
