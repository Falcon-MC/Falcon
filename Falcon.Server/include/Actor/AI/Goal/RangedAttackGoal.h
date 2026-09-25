#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <string>

class RangedAttackGoal : public Goal {
public:
    RangedAttackGoal(float speed, float range, int32_t minInterval, int32_t maxInterval, std::string projectile);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    int32_t _nextInterval() const;

    float mSpeed;
    float mRange;
    int32_t mMinInterval;
    int32_t mMaxInterval;
    std::string mProjectile;
    int32_t mCooldown = 0;
    int32_t mTicksUntilRepath = 0;
};
