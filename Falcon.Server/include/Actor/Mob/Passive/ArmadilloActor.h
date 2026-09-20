#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class ArmadilloActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:armadillo";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.7f, 0.65f}; }

    float getDefaultMaxHealth() const override { return 12.0f; }
};
