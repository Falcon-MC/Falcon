#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class HappyGhastActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:happy_ghast";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{4.0f, 4.0f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }
};
