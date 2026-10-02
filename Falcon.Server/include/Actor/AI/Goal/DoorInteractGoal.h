#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3i.h"

class BlockState;
class Level;

class DoorInteractGoal : public Goal {
public:
    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    const BlockState *getDoorState(Level &level) const;

    bool isDoorClosed(Level &level) const;

    Vector3i mDoorPosition;

private:
    bool _findDoor(Level &level, MobActor &mob);

    float mDirectionX = 0.0f;
    float mDirectionZ = 0.0f;
    bool mPassed = false;
};
