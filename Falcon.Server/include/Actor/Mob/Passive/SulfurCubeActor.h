#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class SulfurCubeActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:sulfur_cube";

    int getExperienceDrop() const override { return randomRange(1, 2); }

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.98f, 0.98f}; }

    float getDefaultMaxHealth() const override { return 8.0f; }
};
