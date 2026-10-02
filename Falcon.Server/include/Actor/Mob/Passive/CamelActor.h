#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class CamelActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:camel";

    using PassiveActor::PassiveActor;

    static constexpr float CAMEL_STEP_HEIGHT = 1.5625f;

    static constexpr float RIDE_SPRINT_MULTIPLIER = 2.5f;

    float getRideSprintMultiplier() const override {
        return RIDE_SPRINT_MULTIPLIER;
    }

    ActorSize getSize() const override { return ActorSize{1.7f, 2.375f}; }

    PhysicsComponent getPhysics() const override {
        PhysicsComponent physics = PassiveActor::getPhysics();
        physics.mStepHeight = CAMEL_STEP_HEIGHT;
        return physics;
    }

    float getDefaultMaxHealth() const override { return 32.0f; }
};
