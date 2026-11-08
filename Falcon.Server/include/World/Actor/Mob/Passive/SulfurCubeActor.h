#pragma once

#include "World/Actor/Mob/Passive/PassiveActor.h"

// everything else comes from its definition: archetype groups, physics attributes, area attack, explode
class SulfurCubeActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:sulfur_cube";

    using PassiveActor::PassiveActor;

    // the small and medium groups swap the collision box
    ActorSize getSize() const override {
        return definitionSize(ActorSize{0.98f, 0.98f});
    }

    float getDefaultMaxHealth() const override {
        return 8.0f;
    }
};
