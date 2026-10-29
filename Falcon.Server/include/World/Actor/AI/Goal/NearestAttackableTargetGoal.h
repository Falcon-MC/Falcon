#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>
#include <vector>

class Actor;

class NearestAttackableTargetGoal : public Goal {
public:
    struct Entry {
        std::shared_ptr<json::Value> mFilters;
        float mMaxDistance = 0.0f;
    };

    NearestAttackableTargetGoal(float range, std::vector<Entry> entries, int32_t scanInterval);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Actor *_findNearest(ServerNetworkHandler &owner, MobActor &mob) const;

    bool _matches(ServerNetworkHandler &owner, MobActor &mob, const Actor &candidate) const;

    float mRangeSquared;
    int32_t mScanInterval;
    // Found by canUse and consumed by start in the same goal selection, so the scan is not run twice.
    uint64_t mFoundTargetRuntimeId = 0;
    float mMaxRangeSquared;
    std::vector<Entry> mEntries;
};
