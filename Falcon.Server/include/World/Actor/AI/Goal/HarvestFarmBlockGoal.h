#pragma once

#include "World/Actor/AI/Goal/BaseMoveToBlockGoal.h"

#include <cstdint>

// minecraft:behavior.harvest_farm_block: a farmer breaks the ripe crop on farmland and replants it, or plants a
// seed it carries on bare farmland
class HarvestFarmBlockGoal final : public BaseMoveToBlockGoal {
public:
    struct Timing {
        // the first search waits up to this many ticks
        int32_t mMaxTicksBeforeSearch = 0;
        // a failed search waits up to this many ticks
        int32_t mSearchCooldownTicks = 0;
        int32_t mTicksUntilNewTask = 0;
    };

    HarvestFarmBlockGoal(Settings settings, Timing timing);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

    // the inventory slot holding the first item with a crop to plant; -1 for none
    static int findSeedSlot(const MobActor &mob);

    // the item at the slot plants its crop above the farmland and loses one
    static bool plantSeed(ServerNetworkHandler &owner, MobActor &mob, int slot, const Vector3i &farmland);

protected:
    bool isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) override;

private:
    enum class Task : int32_t { None = -1, Harvest = 0, Plant = 1 };

    Timing mTiming;
    int32_t mCooldownTicks = 0;
    int mSeedSlot = -1;
    Task mTask = Task::None;
};
