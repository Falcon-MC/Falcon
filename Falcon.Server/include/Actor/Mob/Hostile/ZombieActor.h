#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class ZombieActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:zombie";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }

    bool burnsInDaylight() const override {
        return true;
    }

    float getAttackDamage(Difficulty difficulty) const override;

protected:
    void registerGoals(GoalSelector &goalSelector) override;
};
