#pragma once

#include "World/Actor/AI/Goal/Goal.h"

class Actor;

// minecraft:behavior.charge_attack: a flying mob dashes at its target's eyes and strikes on contact
class ChargeAttackGoal : public Goal {
public:
    struct Settings {
        float mMinDistance = 2.0f;
        float mMaxDistance = 3.0f;
        float mSuccessRate = 0.1428f;
        float mSpeedMultiplier = 1.0f;
    };

    explicit ChargeAttackGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Settings mSettings;
};
