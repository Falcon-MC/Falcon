#pragma once

#include "World/Actor/Mob/Passive/PassiveActor.h"

class SheepActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:sheep";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override {
        return ActorSize{0.9f, 1.3f};
    }

    float getDefaultMaxHealth() const override {
        return 8.0f;
    }

    // a dispenser's shears: the game names the event and the loot table in its code, not in the definition
    void shearFromDispenser(ServerNetworkHandler &owner);
};
