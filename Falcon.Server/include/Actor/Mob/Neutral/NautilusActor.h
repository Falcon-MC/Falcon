#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class NautilusActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:nautilus";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{0.875f, 0.95f}; }

    float getDefaultMaxHealth() const override { return 15.0f; }
};
