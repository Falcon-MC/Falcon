#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class ZombieVillagerV2Actor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:zombie_villager_v2";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }

    bool burnsInDaylight() const override {
        return true;
    }
};
