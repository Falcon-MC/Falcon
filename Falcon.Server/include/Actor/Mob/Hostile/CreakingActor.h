#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class CreakingActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:creaking";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{1.0f, 2.5f}; }

    float getDefaultMaxHealth() const override { return 1.0f; }

    int getExperienceDrop() const override { return 0; }
};
