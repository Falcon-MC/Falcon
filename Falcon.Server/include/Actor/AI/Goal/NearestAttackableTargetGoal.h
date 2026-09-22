#pragma once

#include "Actor/AI/Goal/Goal.h"

class ServerPlayer;

class NearestAttackableTargetGoal : public Goal {
public:
    explicit NearestAttackableTargetGoal(float range);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    ServerPlayer *_findNearest(ServerNetworkHandler &owner, const MobActor &mob) const;

    bool _inRange(const MobActor &mob, const ServerPlayer &player) const;

    float mRangeSquared;
};
