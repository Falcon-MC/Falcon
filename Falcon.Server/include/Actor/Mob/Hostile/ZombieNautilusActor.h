#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class ZombieNautilusActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:zombie_nautilus";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.875f, 0.95f}; }

    float getDefaultMaxHealth() const override { return 15.0f; }

    bool preventsSleep() const override {
        return false;
    }

    int getExperienceDrop() const override { return randomRange(1, 3); }

    bool burnsInDaylight() const override {
        return true;
    }
};
