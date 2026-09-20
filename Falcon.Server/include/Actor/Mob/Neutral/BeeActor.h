#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class BeeActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:bee";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{0.55f, 0.5f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }
};
