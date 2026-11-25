#pragma once

#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>

class VillagerV2Actor;

// minecraft:behavior.make_love: a villager picks a fed partner it can walk to, waits for it and spawns the baby;
// villagers only
class MakeLoveGoal final : public Goal {
public:
    MakeLoveGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

    // the love partner of a villager, while it is still a villager
    static VillagerV2Actor *findPartner(ServerNetworkHandler &owner, const VillagerV2Actor &villager);

private:
    VillagerV2Actor *_choosePartner(ServerNetworkHandler &owner, VillagerV2Actor &villager) const;

    void _spawnBaby(ServerNetworkHandler &owner, VillagerV2Actor &villager, VillagerV2Actor &partner);

    int32_t mLoveTicks = 0;
};
