#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <memory>

// minecraft:behavior.offer_flower: holds a flower out to a mob that can take one
class OfferFlowerGoal : public Goal {
public:
    struct Settings {
        float mChanceToStart = 0.0f;
        int32_t mDurationTicks = 0;
        Vector3f mSearchArea;
        std::shared_ptr<json::Value> mFilters;
    };

    explicit OfferFlowerGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Settings mSettings;
    int64_t mTargetId = 0;
    int64_t mEndTick = 0;
};
