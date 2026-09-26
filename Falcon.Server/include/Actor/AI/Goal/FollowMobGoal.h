#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>

class Actor;

class FollowMobGoal : public Goal {
public:
    FollowMobGoal(float speed, float searchRange, float stopDistance, std::shared_ptr<json::Value> filters);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Actor *_findLeader(ServerNetworkHandler &owner, const MobActor &mob) const;

    Actor *_resolveLeader(ServerNetworkHandler &owner) const;

    bool _accepts(ServerNetworkHandler &owner, const MobActor &mob, const Actor &candidate) const;

    float mSpeed;
    float mSearchRange;
    float mStopDistance;
    std::shared_ptr<json::Value> mFilters;
    int64_t mLeaderId = 0;
    int32_t mTicksUntilRepath = 0;
};
