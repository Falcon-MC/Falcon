#pragma once

#include "World/Actor/Mob/Neutral/NeutralActor.h"

class PufferfishActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:pufferfish";

    using NeutralActor::NeutralActor;

    // no puff states yet: the full_puff area attack would hurt from a deflated fish
    bool hasOwnAreaAttack() const override {
        return true;
    }

    ActorSize getSize() const override {
        return ActorSize{0.8f, 0.8f};
    }

    float getDefaultMaxHealth() const override {
        return 3.0f;
    }
};
