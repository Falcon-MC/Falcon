#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class Actor;
class SpearItem;

class UseKineticWeaponGoal : public Goal {
public:
    struct Settings {
        float mSpeed = 0.25f;
        float mApproachDistance = 10.0f;
        float mMinRepositionDistance = 6.0f;
        float mMaxRepositionDistance = 7.0f;
        float mMinCooldownDistance = 9.0f;
        float mMaxCooldownDistance = 11.0f;
        float mReachMultiplier = 1.0f;
        float mMinSpeedMultiplier = 0.0f;
        bool mHijackMountNavigation = false;
    };

    explicit UseKineticWeaponGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    enum class Phase {
        Approach,
        Charge,
        Reposition,
        Cooldown
    };

    static const SpearItem *_heldSpear(const MobActor &mob);

    MobActor &_navigator(ServerNetworkHandler &owner, MobActor &mob) const;

    void _moveTo(ServerNetworkHandler &owner, MobActor &mob, const Vector3f &target, float speed) const;

    void _setCharging(ServerNetworkHandler &owner, MobActor &mob, bool charging) const;

    void _strike(ServerNetworkHandler &owner, MobActor &mob, Actor &target, const SpearItem &spear, float speed);

    Vector3f _awayFrom(const MobActor &mob, const Actor &target, float distance) const;

    float _roll(float minimum, float maximum) const;

    Settings mSettings;
    Phase mPhase = Phase::Approach;
    Vector3f mLastPosition;
    float mPhaseDistance = 0.0f;
    int32_t mPhaseTicks = 0;
    int32_t mRepathTicks = 0;
};
