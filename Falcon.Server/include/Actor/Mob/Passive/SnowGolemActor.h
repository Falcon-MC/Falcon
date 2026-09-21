#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class SnowGolemActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:snow_golem";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.4f, 1.8f}; }

    float getDefaultMaxHealth() const override { return 4.0f; }

    int getExperienceDrop() const override { return 0; }
};
