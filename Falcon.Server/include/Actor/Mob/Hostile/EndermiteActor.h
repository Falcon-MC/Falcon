#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class EndermiteActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:endermite";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.4f, 0.3f}; }

    float getDefaultMaxHealth() const override { return 8.0f; }

    int getExperienceDrop() const override { return 3; }
};
