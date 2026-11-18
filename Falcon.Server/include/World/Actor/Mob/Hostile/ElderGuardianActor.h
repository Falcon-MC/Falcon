#pragma once

#include "World/Actor/Mob/Hostile/HostileActor.h"

#include <cstdint>

class ElderGuardianActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:elder_guardian";

    using HostileActor::HostileActor;

    void tick(ServerNetworkHandler &owner) override;

    ActorSize getSize() const override {
        return ActorSize{1.99f, 1.99f};
    }

    float getDefaultMaxHealth() const override {
        return 80.0f;
    }

    int getExperienceDrop() const override {
        return 10;
    }

    // target data drives the laser: GuardianAttackGoal sets it only while charging
    bool syncsTargetData() const override {
        return false;
    }

private:
    // every minute the curse reaches the players around: mining fatigue and the ghost on their screen
    void _curse(ServerNetworkHandler &owner);

    uint32_t mTicks = 0;
};
