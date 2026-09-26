#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class WardenActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:warden";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.9f, 2.9f}; }

    float getDefaultMaxHealth() const override { return 500.0f; }

    bool preventsSleep() const override {
        return false;
    }
};
