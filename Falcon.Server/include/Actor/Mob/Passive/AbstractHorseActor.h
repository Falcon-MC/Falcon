#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class AbstractHorseActor : public PassiveActor {
public:
    static constexpr int MINIMUM_HEALTH = 15;

    static constexpr int MAXIMUM_HEALTH = 30;

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{1.4f, 1.6f}; }

    float getDefaultMaxHealth() const override { return (float) MINIMUM_HEALTH; }

    float resolveMaxHealth(Difficulty difficulty) const override {
        (void) difficulty;
        return (float) randomRange(MINIMUM_HEALTH, MAXIMUM_HEALTH);
    }
};
