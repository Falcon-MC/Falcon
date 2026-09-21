#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class SpiderActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:spider";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{1.4f, 0.9f}; }

    float getDefaultMaxHealth() const override { return 16.0f; }
};
