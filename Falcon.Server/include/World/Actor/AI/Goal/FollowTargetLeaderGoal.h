#pragma once

#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>

class Actor;

// follows a leader picked by filters at a distance; the leader is kept on the mob (TargetCaptainID)
class FollowTargetLeaderGoal : public Goal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        float mWithinRadius = 0.0f;
        float mFollowDistance = 0.0f;
        bool mAlwaysLookForLeader = false;
        int32_t mSearchCooldownTicks = 20;
        std::shared_ptr<json::Value> mLeaderFilters;
    };

    explicit FollowTargetLeaderGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Actor *_leader(ServerNetworkHandler &owner, const MobActor &mob) const;

    Actor *_search(ServerNetworkHandler &owner, MobActor &mob) const;

    bool _isLeader(ServerNetworkHandler &owner, const MobActor &mob, const Actor &candidate) const;

    bool _inBand(const MobActor &mob, const Actor &leader) const;

    void _moveTowards(ServerNetworkHandler &owner, MobActor &mob);

    Settings mSettings;
    int32_t mSearches = 0;
    int64_t mNextSearchTick = 0;
};
