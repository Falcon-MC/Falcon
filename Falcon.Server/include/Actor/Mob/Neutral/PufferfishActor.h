#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class PufferfishActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:pufferfish";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{0.8f, 0.8f}; }

    float getDefaultMaxHealth() const override { return 3.0f; }
};
