#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <string>

class BreedGoal : public Goal {
public:
    explicit BreedGoal(float speed);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    MobActor *_findPartner(ServerNetworkHandler &owner, const MobActor &mob) const;

    MobActor *_partner(ServerNetworkHandler &owner) const;

    void _breed(ServerNetworkHandler &owner, MobActor &mob, MobActor &partner);

    float mSpeed;
    int64_t mPartnerId = 0;
    int32_t mLoveTime = 0;
};
