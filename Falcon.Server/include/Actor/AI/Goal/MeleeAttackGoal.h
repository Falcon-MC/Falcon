#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class Actor;

class MeleeAttackGoal : public Goal {
public:
    MeleeAttackGoal(float speed, float maxRange, int32_t coolDown, float attackRangeSquared);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    virtual void _tryAttack(ServerNetworkHandler &owner, MobActor &mob, Actor &target);

    void _attack(ServerNetworkHandler &owner, MobActor &mob, Actor &target);

    int32_t mCoolDown;
    float mAttackRangeSquared;
    int32_t mTicksSinceAttack = 0;

private:
    float mSpeed;
    float mMaxRangeSquared;
    int32_t mLastTargetX = 0;
    int32_t mLastTargetY = 0;
    int32_t mLastTargetZ = 0;
    bool mHasLastTarget = false;
};
