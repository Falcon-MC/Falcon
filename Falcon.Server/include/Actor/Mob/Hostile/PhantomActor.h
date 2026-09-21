#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class PhantomActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:phantom";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.9f, 0.5f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }
};
