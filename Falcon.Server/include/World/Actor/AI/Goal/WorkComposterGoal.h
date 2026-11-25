#pragma once

#include "World/Actor/AI/Goal/WorkGoal.h"

#include <cstdint>

// minecraft:behavior.work_composter: the work goal of a farmer, who uses its composter instead of making the
// job sound
class WorkComposterGoal final : public WorkGoal {
public:
    struct Composting {
        int32_t mUseBlockMin = 0;
        int32_t mUseBlockMax = 0;
        int32_t mBlockInteractionMax = 0;
        // -1: no limit
        int32_t mItemsPerUseMax = 0;
        // the seeds the farmer keeps of each kind
        int32_t mMinItemCount = 0;
        bool mCanEmpty = false;
        bool mCanFill = false;
    };

    WorkComposterGoal(Settings settings, Composting composting);

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    void useWorkstation(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    int64_t _nextUseTick(ServerNetworkHandler &owner) const;

    bool _fill(ServerNetworkHandler &owner, MobActor &mob);

    Composting mComposting;
    int64_t mNextUseTick = 0;
    int32_t mInteractions = 0;
};
