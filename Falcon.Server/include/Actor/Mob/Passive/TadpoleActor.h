#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class TadpoleActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:tadpole";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 0.8f}; }

    float getDefaultMaxHealth() const override { return 6.0f; }
};
