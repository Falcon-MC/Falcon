#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class CreakingActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:creaking";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.9f, 2.7f}; }

    float getDefaultMaxHealth() const override { return 1.0f; }

    bool preventsSleep() const override {
        return false;
    }

    int getExperienceDrop() const override { return 0; }
};
