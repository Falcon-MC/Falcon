#pragma once

#include "Core/Json/Json.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>
#include <vector>

// minecraft:behavior.share_items: a mob with more of a shareable item than it keeps carries some to a nearby mob
// that wants it and throws it over
class ShareItemsGoal final : public Goal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        // the search cube's half side, whole blocks
        int32_t mMaxDistance = 0;
        std::vector<CompiledEntityFilter> mFilters;
    };

    explicit ShareItemsGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Settings mSettings;
    ItemStack mItem;
    int32_t mThrowTicks = 0;
};
