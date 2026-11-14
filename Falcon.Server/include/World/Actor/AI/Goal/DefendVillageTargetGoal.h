#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>
#include <vector>

class Actor;

// minecraft:behavior.defend_village_target: a dweller turns on the closest aggressor of its village; a player only
// once the village holds them hostile
class DefendVillageTargetGoal : public Goal {
public:
    DefendVillageTargetGoal(float attackChance, float range, std::vector<std::shared_ptr<json::Value>> filters);

    // the aggressor the goal would pick, before the chance roll and the entity_types filters
    static Actor *pickAggressor(ServerNetworkHandler &owner, const MobActor &mob);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    float mAttackChance;
    float mRangeSquared;
    std::vector<std::shared_ptr<json::Value>> mFilters;
    uint64_t mFoundRuntimeId = 0;
};
