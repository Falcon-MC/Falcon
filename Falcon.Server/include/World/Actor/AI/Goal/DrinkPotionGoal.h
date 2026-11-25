#pragma once

#include "Core/Json/Json.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>
#include <vector>

// minecraft:behavior.drink_potion: the first potion whose chance and filters allow it is held and drunk, its
// effects taken when the drinking ends
class DrinkPotionGoal : public Goal {
public:
    struct Potion {
        int32_t mId = 0;
        float mChance = 1.0f;
        CompiledEntityFilter mFilters;
    };

    explicit DrinkPotionGoal(std::vector<Potion> potions);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    std::vector<Potion> mPotions;
    int mCalls = 0;
    int64_t mEndTick = 0;
};
