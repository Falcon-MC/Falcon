#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class WolfActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:wolf";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 0.8f}; }

    float getDefaultMaxHealth() const override { return 8.0f; }
};
