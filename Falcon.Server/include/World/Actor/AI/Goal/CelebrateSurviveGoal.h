#pragma once

#include "Core/Json/Json.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>

// minecraft:behavior.celebrate_survive: villagers that lived through a raid set off fireworks for a while, then
// fire on_celebration_end_event
class CelebrateSurviveGoal final : public Goal {
public:
    struct Settings {
        int32_t mDurationTicks = 0;
        int32_t mFireworksMinTicks = 0;
        int32_t mFireworksMaxTicks = 0;
        std::shared_ptr<json::Value> mOnEnd;
    };

    explicit CelebrateSurviveGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Settings mSettings;
    int32_t mTicksLeft = 0;
    int32_t mNextFirework = 0;
};
