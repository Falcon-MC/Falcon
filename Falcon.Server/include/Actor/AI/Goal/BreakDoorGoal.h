#pragma once

#include "Actor/AI/Goal/DoorInteractGoal.h"

#include <cstdint>

class BreakDoorGoal : public DoorInteractGoal {
public:
    explicit BreakDoorGoal(int32_t breakTicks);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _canBreakDoors(ServerNetworkHandler &owner, Level &level) const;

    void _broadcastBreakEvent(ServerNetworkHandler &owner, Level &level, int32_t event, int32_t data) const;

    int32_t mBreakTicks;
    int32_t mElapsedTicks = 0;
};
