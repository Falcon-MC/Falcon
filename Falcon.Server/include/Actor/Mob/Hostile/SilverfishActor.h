#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class SilverfishActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:silverfish";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.4f, 0.3f}; }

    float getDefaultMaxHealth() const override { return 8.0f; }
};
