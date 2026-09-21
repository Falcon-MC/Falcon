#pragma once

#include "Actor/Mob/MobActor.h"

class PassiveActor : public MobActor {
public:
    static constexpr int MINIMUM_EXPERIENCE = 1;

    static constexpr int MAXIMUM_EXPERIENCE = 3;

    using MobActor::MobActor;

    ActorCategory getCategory() const override { return ActorCategory::Passive; }

    static constexpr float ANIMAL_STEP_HEIGHT = 0.5f;

    int getExperienceDrop() const override {
        return randomRange(MINIMUM_EXPERIENCE, MAXIMUM_EXPERIENCE);
    }

    PhysicsComponent getPhysics() const override {
        PhysicsComponent physics = MobActor::getPhysics();
        physics.mStepHeight = ANIMAL_STEP_HEIGHT;
        return physics;
    }
};
