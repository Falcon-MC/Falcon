#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class BlazeActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:blaze";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.5f, 1.8f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }

    int getExperienceDrop() const override { return 10; }

    bool hasGravity() const override {
        return false;
    }
};
