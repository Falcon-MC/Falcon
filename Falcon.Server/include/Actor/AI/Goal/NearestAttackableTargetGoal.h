#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <memory>
#include <vector>

class Actor;

class NearestAttackableTargetGoal : public Goal {
public:
    struct Entry {
        std::shared_ptr<json::Value> mFilters;
        float mMaxDistance = 0.0f;
    };

    NearestAttackableTargetGoal(float range, std::vector<Entry> entries);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Actor *_findNearest(ServerNetworkHandler &owner, MobActor &mob) const;

    bool _matches(ServerNetworkHandler &owner, MobActor &mob, const Actor &candidate) const;

    float mRangeSquared;
    std::vector<Entry> mEntries;
};
