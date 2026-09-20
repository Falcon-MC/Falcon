#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class AllayActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:allay";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 0.6f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }

    int getExperienceDrop() const override { return 0; }
};
