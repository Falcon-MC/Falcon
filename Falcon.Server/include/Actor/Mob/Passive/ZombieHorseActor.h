#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class ZombieHorseActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:zombie_horse";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{1.4f, 1.6f}; }

    float getDefaultMaxHealth() const override { return 15.0f; }

    bool burnsInDaylight() const override {
        return true;
    }
};
