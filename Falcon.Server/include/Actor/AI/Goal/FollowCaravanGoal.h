#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>

class Actor;

class FollowCaravanGoal : public Goal {
public:
    FollowCaravanGoal(float movementSpeed, float speedMultiplier, int32_t entityCount,
                      std::shared_ptr<json::Value> filters);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Actor *_findHead(ServerNetworkHandler &owner, const MobActor &mob, bool inCaravan) const;

    bool _isChainLeashed(ServerNetworkHandler &owner, int64_t uniqueId, int32_t depth) const;

    float mMovementSpeed;
    float mBaseSpeedMultiplier;
    float mSpeedMultiplier;
    int32_t mEntityCount;
    std::shared_ptr<json::Value> mFilters;
    int64_t mHeadId = 0;
    int32_t mDistanceCheckTicks = 0;
    int32_t mTicksUntilRepath = 0;
};
