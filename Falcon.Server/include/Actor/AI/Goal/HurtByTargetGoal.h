#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>

class HurtByTargetGoal : public Goal {
public:
    explicit HurtByTargetGoal(std::shared_ptr<json::Value> filters = nullptr);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    std::shared_ptr<json::Value> mFilters;
    uint32_t mHandledHurtCount = 0;
};
