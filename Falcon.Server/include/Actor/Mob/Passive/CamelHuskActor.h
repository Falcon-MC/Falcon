#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class CamelHuskActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:camel_husk";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{1.7f, 2.375f}; }

    float getDefaultMaxHealth() const override { return 32.0f; }

    int getExperienceDrop() const override { return 0; }
};
