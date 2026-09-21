#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class DolphinActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:dolphin";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{0.9f, 0.6f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }

    int getExperienceDrop() const override { return 0; }
};
