#pragma once

#include "World/Actor/AI/Goal/MeleeAttackGoal.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

// minecraft:behavior.ocelotattack: the cat and ocelot stalk their target crouched, sprint at it from close by and
// strike once in reach
class OcelotAttackGoal : public MeleeAttackGoal {
public:
    struct Settings {
        int32_t mCoolDownTicks = 20;
        float mMaxDistance = 15.0f;
        float mMaxSneakRange = 15.0f;
        float mMaxSprintRange = 4.0f;
        float mReachMultiplier = 2.0f;
        float mWalkSpeed = 0.0f;
        float mSneakSpeed = 0.0f;
        float mSprintSpeed = 0.0f;
    };

    explicit OcelotAttackGoal(Settings settings);

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    enum class Gait : uint8_t { None, Walk, Sneak, Sprint };

    void _setGait(ServerNetworkHandler &owner, MobActor &mob, Gait gait);

    Settings mSettings;
    Gait mGait = Gait::None;
    Vector3i mLastTargetBlock;
};
