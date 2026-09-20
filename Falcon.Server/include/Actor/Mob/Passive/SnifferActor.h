#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class SnifferActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:sniffer";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{1.9f, 1.75f}; }

    float getDefaultMaxHealth() const override { return 14.0f; }
};
