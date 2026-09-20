#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class VexActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:vex";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.4f, 0.8f}; }

    float getDefaultMaxHealth() const override { return 14.0f; }
};
