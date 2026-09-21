#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class AbstractHorseActor : public PassiveActor {
public:
    static constexpr int MINIMUM_HEALTH = 15;

    static constexpr int MAXIMUM_HEALTH = 30;

    using PassiveActor::PassiveActor;

    static constexpr float HORSE_STEP_HEIGHT = 1.0625f;

    ActorSize getSize() const override { return ActorSize{1.4f, 1.6f}; }

    PhysicsComponent getPhysics() const override {
        PhysicsComponent physics = PassiveActor::getPhysics();
        physics.mStepHeight = HORSE_STEP_HEIGHT;
        return physics;
    }

    float getDefaultMaxHealth() const override { return (float) MINIMUM_HEALTH; }

    float resolveMaxHealth(Difficulty difficulty) const override {
        (void) difficulty;
        return (float) randomRange(MINIMUM_HEALTH, MAXIMUM_HEALTH);
    }
};
