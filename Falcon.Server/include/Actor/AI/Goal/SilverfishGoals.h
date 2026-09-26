#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class SilverfishMergeWithStoneGoal : public Goal {
public:
    SilverfishMergeWithStoneGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Vector3i mPosition;
    BlockState mInfestedState;
};

class SilverfishWakeUpFriendsGoal : public Goal {
public:
    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    static void _wakeFriends(ServerNetworkHandler &owner, MobActor &mob);

    uint32_t mHandledHurtCount = 0;
    int32_t mDelayTicks = 0;
};
