#pragma once

#include "Actor/AI/Goal/DoorInteractGoal.h"

#include <cstdint>

class OpenDoorGoal : public DoorInteractGoal {
public:
    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    int32_t mForgetTicks = 0;
    bool mOpened = false;
};
