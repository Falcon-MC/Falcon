#pragma once

#include "Core/Json/Json.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "Core/Math/Vector3f.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>
#include <vector>

// minecraft:behavior.play: a child chases a friend found nearby, or runs to random spots when none is around
class PlayGoal final : public Goal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        float mChanceToStart = 0.0f;
        float mFollowDistance = 0.0f;
        Vector3f mFriendSearchArea;
        int32_t mMaxPlayTicks = 0;
        int32_t mRandomRange = 0;
        int32_t mRandomHeight = 0;
        std::vector<CompiledEntityFilter> mFriendFilters;
    };

    explicit PlayGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    int64_t _findFriend(ServerNetworkHandler &owner, MobActor &mob) const;

    Settings mSettings;
    int64_t mFriendId = 0;
    int32_t mTicksLeft = 0;
};
