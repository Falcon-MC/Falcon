#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <memory>

class Level;

class TeleportToOwnerGoal : public Goal {
public:
    explicit TeleportToOwnerGoal(std::shared_ptr<json::Value> filters);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    static bool _isSafe(Level &level, int32_t x, int32_t y, int32_t z);

    std::shared_ptr<json::Value> mFilters;
};
