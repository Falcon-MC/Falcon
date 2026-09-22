#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class BatActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:bat";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.5f, 0.9f}; }

    float getDefaultMaxHealth() const override { return 6.0f; }

    int getExperienceDrop() const override { return 0; }

    bool hasGravity() const override {
        return false;
    }
};
