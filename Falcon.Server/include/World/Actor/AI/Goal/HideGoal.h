#pragma once

#include "World/Actor/AI/Goal/MoveToPoiGoal.h"

#include <cstdint>

// minecraft:behavior.hide: a villager of a raided village runs to its point of interest and stays there for the
// raid, and for the duration at least
class HideGoal final : public MoveToPoiGoal {
public:
    HideGoal(float speed, PoiType type, int32_t durationTicks);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

    static bool isVillageRaided(const MobActor &mob);

private:
    int32_t mDurationTicks;
    int32_t mHiddenTicks = 0;
};
