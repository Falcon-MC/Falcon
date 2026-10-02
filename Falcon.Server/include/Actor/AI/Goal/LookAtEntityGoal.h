#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>

class Actor;

class LookAtEntityGoal : public Goal {
public:
    struct Settings {
        float mRange = 8.0f;
        float mProbability = 0.02f;
        int32_t mMinLookTicks = 40;
        int32_t mMaxLookTicks = 80;
        float mHorizontalAngle = 360.0f;
        std::shared_ptr<json::Value> mFilters;
    };

    explicit LookAtEntityGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _accepts(ServerNetworkHandler &owner, const MobActor &mob, const Actor &candidate) const;

    Actor *_findNearest(ServerNetworkHandler &owner, const MobActor &mob) const;

    Actor *_current(ServerNetworkHandler &owner) const;

    Settings mSettings;
    uint64_t mTargetRuntimeId = 0;
    int32_t mRemainingTicks = 0;
};
