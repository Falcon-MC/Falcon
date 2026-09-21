#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class BoggedActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:bogged";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 16.0f; }
};
