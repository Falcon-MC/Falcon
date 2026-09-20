#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class GoatActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:goat";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{0.9f, 1.3f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }
};
