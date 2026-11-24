#pragma once

#include "World/Actor/AI/Goal/BaseMoveToBlockGoal.h"

#include <cstdint>

// minecraft:behavior.fertilize_farm_block: a farmer spends its fertilizer on the unripe crops of nearby farmland
class FertilizeFarmBlockGoal final : public BaseMoveToBlockGoal {
public:
    FertilizeFarmBlockGoal(Settings settings, int32_t maxUses, int32_t searchCooldownTicks);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) override;

private:
    int32_t mMaxUses;
    int32_t mSearchCooldownTicks;
    int64_t mNextSearchTick = 0;
    int mFertilizerSlot = -1;
    int32_t mUses = 0;
};
