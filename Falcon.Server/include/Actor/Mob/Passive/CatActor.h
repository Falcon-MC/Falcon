#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class CatActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:cat";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.48f, 0.56f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }
};
