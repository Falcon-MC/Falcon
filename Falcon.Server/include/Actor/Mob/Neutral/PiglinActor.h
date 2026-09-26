#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class PiglinActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:piglin";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 16.0f; }

    bool preventsSleep() const override {
        return !getFlags().get(ActorFlag::Baby);
    }

    int getExperienceDrop() const override { return 5; }
};
