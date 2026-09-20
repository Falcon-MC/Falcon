#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class FrogActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:frog";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.5f, 0.55f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }
};
