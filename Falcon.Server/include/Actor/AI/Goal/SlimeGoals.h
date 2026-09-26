#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class AbstractSlimeActor;

class SlimeGoal : public Goal {
protected:
    static AbstractSlimeActor *slimeOf(MobActor &mob);
};

class SlimeFloatGoal : public SlimeGoal {
public:
    SlimeFloatGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;
};

class SlimeKeepOnJumpingGoal : public SlimeGoal {
public:
    SlimeKeepOnJumpingGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;
};

class SlimeRandomDirectionGoal : public SlimeGoal {
public:
    SlimeRandomDirectionGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    int32_t mNextChange = 0;
};

class SlimeAttackGoal : public SlimeGoal {
public:
    SlimeAttackGoal();

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    int32_t mTiredTicks = 0;
};
