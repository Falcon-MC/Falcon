#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class TropicalFishActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:tropicalfish";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.5f, 0.4f}; }

    float getDefaultMaxHealth() const override { return 6.0f; }
};
