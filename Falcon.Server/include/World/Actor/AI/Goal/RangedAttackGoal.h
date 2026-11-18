#pragma once

#include "World/Actor/AI/Goal/Goal.h"

#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class Actor;

class RangedAttackGoal : public Goal {
public:
    // field order and defaults of the game's definition
    struct Settings {
        float mSpeed = 1.0f;
        float mAttackRadius = 0.0f;
        float mAttackRadiusMin = 0.0f;
        int32_t mIntervalMinTicks = 0;
        int32_t mIntervalMaxTicks = 0;
        int32_t mBurstShots = 1;
        int32_t mBurstIntervalTicks = 0;
        int32_t mTargetInSightTicks = 20;
        int32_t mChargeChargedTriggerTicks = 0;
        int32_t mChargeShootTriggerTicks = 0;
        float mRangedFov = 90.0f;
        bool mFollowTarget = false;
        bool mSetPersistent = false;
        bool mSwing = false;
        // not a definition field: the goal needs a minecraft:navigation.* component
        bool mHasNavigation = true;
    };

    explicit RangedAttackGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _hasShooter(const MobActor &mob);

    void _move(MobActor &mob, const Actor &target, float distanceSquared);

    void _follow(MobActor &mob, const Vector3f &targetPosition);

    void _tickCharge(ServerNetworkHandler &owner, MobActor &mob, bool canSee);

    void _tickBurst(ServerNetworkHandler &owner, MobActor &mob, Actor &target, float distanceSquared, bool canSee);

    int32_t _intervalFor(float distanceSquared) const;

    bool _inFieldOfView(const MobActor &mob, const Actor &target) const;

    Settings mSettings;
    const json::Value *mShooter = nullptr;
    bool mShooterUsable = false;
    bool mHeldCharged = false;
    int32_t mAttackTime = 0;
    int32_t mSeeTime = 0;
    int32_t mBurstRemaining = 0;
    int32_t mBurstCooldown = 0;
    int32_t mChargeTime = 0;
    bool mHasPathTarget = false;
    int32_t mPathTargetX = 0;
    int32_t mPathTargetY = 0;
    int32_t mPathTargetZ = 0;
};
