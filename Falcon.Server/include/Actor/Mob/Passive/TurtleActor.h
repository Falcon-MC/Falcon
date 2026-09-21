#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class TurtleActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:turtle";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{1.2f, 0.4f}; }

    float getDefaultMaxHealth() const override { return 30.0f; }
};
