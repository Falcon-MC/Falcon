#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class PiglinBruteActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:piglin_brute";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 50.0f; }

    bool preventsSleep() const override {
        return false;
    }
};
