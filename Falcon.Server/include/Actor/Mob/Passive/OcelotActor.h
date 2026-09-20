#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class OcelotActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:ocelot";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 0.7f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }
};
