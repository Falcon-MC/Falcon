#pragma once

#include "World/Actor/Mob/Passive/PassiveActor.h"

// everything else comes from its definition: archetype groups, physics attributes, area attack, explode;
// passive although it spawns as a monster: its families are sulfur_cube, animal and mob, and it stays in peaceful
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

    // the small group sets 4 health, the medium one 8
    bool followsDefinitionHealth() const override {
        return true;
    }

    // only the medium group has an experience_reward: the small cube gives none
    int getExperienceDrop() const override {
        return 0;
    }
};
