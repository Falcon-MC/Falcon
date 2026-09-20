#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class AxolotlActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:axolotl";

    int getExperienceDrop() const override { return 1; }

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.75f, 0.42f}; }

    float getDefaultMaxHealth() const override { return 14.0f; }
};
