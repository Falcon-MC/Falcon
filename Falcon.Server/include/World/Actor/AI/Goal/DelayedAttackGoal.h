#pragma once

#include "World/Actor/AI/Goal/MeleeAttackGoal.h"

#include <cstdint>

// minecraft:behavior.delayed_attack: a melee attack with a wind-up; the hit lands partway through the swing
class DelayedAttackGoal : public MeleeAttackGoal {
public:
    // durations in ticks
    DelayedAttackGoal(float speed, float maxRange, float reachMultiplier, int32_t attackDuration, int32_t hitDelay);

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    void _tryAttack(ServerNetworkHandler &owner, MobActor &mob, Actor &target) override;

private:
    bool _swinging(const MobActor &mob) const;

    void _setSwinging(ServerNetworkHandler &owner, MobActor &mob, bool swinging);

    float mReachMultiplier;
    int32_t mAttackDuration;
    int32_t mHitDelay;
    // counts down through a swing, from the attack duration
    int32_t mSwingTicks;
    // ticks since the last swing ended; a new one needs a whole attack duration
    int32_t mTicksSinceSwing;
};
