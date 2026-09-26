#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class MobActor;

class FindMountGoal : public Goal {
public:
    FindMountGoal(float speed, float radius, int32_t startDelay, int32_t maxFailedAttempts);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    MobActor *_findMount(ServerNetworkHandler &owner, const MobActor &mob) const;

    MobActor *_currentMount(ServerNetworkHandler &owner) const;

    static bool _accepts(const MobActor &vehicle, const MobActor &rider);

    float mSpeed;
    float mRadius;
    int32_t mStartDelay;
    int32_t mMaxFailedAttempts;
    int32_t mDelay = 0;
    int32_t mFailedAttempts = 0;
    int32_t mRepathTicks = 0;
    bool mPathIssued = false;
    int64_t mMountId = 0;
};
