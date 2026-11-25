#pragma once

#include "Core/Json/Json.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <memory>

// minecraft:behavior.trade_with_player: the trader stands still while its trading player stays near
class TradeWithPlayerGoal final : public Goal {
public:
    TradeWithPlayerGoal(std::shared_ptr<json::Value> filters, float maxDistance);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    CompiledEntityFilter mFilters;
    float mMaxDistanceSquared;
};
