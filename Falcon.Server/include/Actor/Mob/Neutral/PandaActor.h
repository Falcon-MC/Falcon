#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class PandaActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:panda";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{1.7f, 1.5f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }
};
