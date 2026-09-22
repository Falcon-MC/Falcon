#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class ParrotActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:parrot";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.5f, 1.0f}; }

    float getDefaultMaxHealth() const override { return 6.0f; }

    bool hasGravity() const override {
        return false;
    }
};
