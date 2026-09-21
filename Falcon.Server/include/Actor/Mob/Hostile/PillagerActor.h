#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class PillagerActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:pillager";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 24.0f; }
};
