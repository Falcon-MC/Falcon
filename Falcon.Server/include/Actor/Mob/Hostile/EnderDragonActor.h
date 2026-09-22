#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class EnderDragonActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:ender_dragon";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{13.0f, 4.0f}; }

    float getDefaultMaxHealth() const override { return 200.0f; }

    int getExperienceDrop() const override { return 12000; }

    bool hasGravity() const override {
        return false;
    }

    int32_t getDeathDuration() const override {
        return 200;
    }

    EntityEventType getDeathEvent() const override {
        return EntityEventType::EnderDragonDeath;
    }
};
