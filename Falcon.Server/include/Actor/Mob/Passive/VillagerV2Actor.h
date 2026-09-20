#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class VillagerV2Actor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:villager_v2";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }

    int getExperienceDrop() const override { return 0; }
};
