#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>

class GoHomeGoal : public Goal {
public:
    GoHomeGoal(float speed, int32_t interval, float goalRadius, std::shared_ptr<json::Value> onHome,
               std::shared_ptr<json::Value> onFailed);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _isHome(const MobActor &mob) const;

    void _fire(ServerNetworkHandler &owner, MobActor &mob, const json::Value *triggers);

    float mSpeed;
    int32_t mInterval;
    float mGoalRadius;
    std::shared_ptr<json::Value> mOnHome;
    std::shared_ptr<json::Value> mOnFailed;
    bool mFinished = false;
};
