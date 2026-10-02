#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class ServerPlayer;

class OwnerTargetGoal : public Goal {
public:
    enum class Mode {
        OwnerHurtBy,
        OwnerHurt
    };

    explicit OwnerTargetGoal(Mode mode);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    ServerPlayer *_findOwner(ServerNetworkHandler &owner, const MobActor &mob) const;

    Mode mMode;
    int64_t mHandledTick = 0;
    int64_t mPendingTick = 0;
    uint64_t mPendingTarget = 0;
};
