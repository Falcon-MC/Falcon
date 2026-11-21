#pragma once

#include "World/Actor/AI/Goal/Goal.h"

#include <memory>
#include <random>

namespace json {
    class Value;
}

// minecraft:behavior.move_around_target: the breeze walks to a spot around its target
class MoveAroundTargetGoal : public Goal {
public:
    struct Settings {
        std::shared_ptr<json::Value> mFilters;
        float mSpeed = 1.0f;
        float mSpreadDegrees = 360.0f;
        float mMinDistance = 4.0f;
        float mMaxDistance = 8.0f;
    };

    explicit MoveAroundTargetGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Settings mSettings;
    std::mt19937 mRandom;
};
