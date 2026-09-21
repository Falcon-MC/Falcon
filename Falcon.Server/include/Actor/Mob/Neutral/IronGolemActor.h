#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class IronGolemActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:iron_golem";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{1.4f, 2.9f}; }

    float getDefaultMaxHealth() const override { return 100.0f; }

    int getExperienceDrop() const override { return 0; }
};
