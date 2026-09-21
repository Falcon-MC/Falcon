#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class PigActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:pig";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.9f, 0.9f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }
};
