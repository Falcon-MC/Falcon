#pragma once

#include "World/Actor/AI/Goal/MeleeAttackGoal.h"

#include <cstdint>

class ServerNetworkHandler;

// minecraft:behavior.melee_box_attack: hits when the attacker's collision box, widened sideways, touches the target's
class MeleeBoxAttackGoal : public MeleeAttackGoal {
public:
    MeleeBoxAttackGoal(float speed, float maxRange, int32_t coolDown, float horizontalReach, float fieldOfView);

    // the target's box against the attacker's box grown by the horizontal reach, never vertically
    static bool isInReach(const MobActor &mob, const Actor &target, float horizontalReach);

    // degrees between the attacker's look and the target's centre, from the attacker's eyes
    static float angleTo(const MobActor &mob, const Actor &target);

protected:
    void _tryAttack(ServerNetworkHandler &owner, MobActor &mob, Actor &target) override;

private:
    bool _isRiding(ServerNetworkHandler &owner, const MobActor &mob, const Actor &target) const;

    bool _canHit(ServerNetworkHandler &owner, MobActor &mob, const Actor &target) const;

    float mHorizontalReach;
    float mFieldOfView;
    // counts down while the goal runs and survives a stop, like the game's
    int32_t mCoolDownLeft;
};
