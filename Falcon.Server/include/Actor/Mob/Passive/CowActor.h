#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class CowActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:cow";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.9f, 1.3f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }
};
