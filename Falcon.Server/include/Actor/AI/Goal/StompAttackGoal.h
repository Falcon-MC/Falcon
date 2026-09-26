#pragma once

#include "Actor/AI/Goal/MeleeAttackGoal.h"

#include <cstdint>

class StompAttackGoal : public MeleeAttackGoal {
public:
    StompAttackGoal(float speed, float maxRange, int32_t coolDown, float stompRangeSquared,
                    float noDamageRangeSquared);

protected:
    void _tryAttack(ServerNetworkHandler &owner, MobActor &mob, Actor &target) override;

private:
    float mNoDamageRangeSquared;
};
