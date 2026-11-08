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
        // -1 inherits the goal's must_see, otherwise 0 or 1
        int8_t mMustSee = -1;
    };

    struct Sight {
        bool mMustSee = false;
        int32_t mForgetTicks = 0;
        bool mReselect = false;
    };

    NearestAttackableTargetGoal(float range, std::vector<Entry> entries, int32_t scanInterval, Sight sight);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Actor *_findNearest(ServerNetworkHandler &owner, MobActor &mob) const;

    bool _matches(ServerNetworkHandler &owner, MobActor &mob, const Actor &candidate) const;

    bool _mustSee(ServerNetworkHandler &owner, MobActor &mob, const Actor &target) const;

    bool _scanDue();

    void _resolveSight(ServerNetworkHandler &owner, MobActor &mob, const Actor &target);

    float mRangeSquared;
    int32_t mScanInterval;
    // set by canUse, consumed by start in the same selection: the scan runs once
    uint64_t mFoundTargetRuntimeId = 0;
    float mMaxRangeSquared;
    std::vector<Entry> mEntries;
    Sight mSight;
    bool mUsesSight = false;
    // the current target's sight rule, resolved once per target
    uint64_t mSightTargetId = 0;
    bool mTargetMustSee = false;
    int32_t mUnseenTicks = 0;
    int32_t mReselectTicks = 0;
};
