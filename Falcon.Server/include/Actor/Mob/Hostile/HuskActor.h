#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class HuskActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:husk";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }
};
