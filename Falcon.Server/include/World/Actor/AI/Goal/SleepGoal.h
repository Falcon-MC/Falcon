#pragma once

#include "World/Actor/AI/Goal/MoveToPoiGoal.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

// minecraft:behavior.sleep: a villager walks to the bed its village gave it and lies in it until the goal ends
class SleepGoal : public MoveToPoiGoal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        float mYOffset = 1.0f;
        bool mCanSleepWhileRiding = false;
        // calls to wait after waking before the bed is tried again
        int32_t mTimeoutTicks = 0;
    };

    explicit SleepGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

    const Vector3f &getSleepPosition() const {
        return mSleepPosition;
    }

private:
    Settings mSettings;
    Vector3i mBed;
    Vector3f mSleepPosition;
    float mSleepYaw = 0.0f;
    Vector3f mLayDownPosition;
    bool mCoolingDown = false;
    int32_t mCooldownCalls = 0;
    bool mWoken = false;
};
