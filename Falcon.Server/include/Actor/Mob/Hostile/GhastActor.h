#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class GhastActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:ghast";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{4.0f, 4.0f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }

    bool preventsSleep() const override {
        return false;
    }

    bool hasGravity() const override {
        return false;
    }
};
