#pragma once

#include "World/Actor/Mob/Hostile/HostileActor.h"

class RavagerActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:ravager";

    using HostileActor::HostileActor;

    // minecraft:collision_box of the game's ravager definition
    ActorSize getSize() const override {
        return ActorSize{1.95f, 2.2f};
    }

    float getDefaultMaxHealth() const override {
        return 100.0f;
    }

    int getExperienceDrop() const override {
        return 20;
    }
};
