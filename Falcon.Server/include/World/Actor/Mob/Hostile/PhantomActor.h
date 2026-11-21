#pragma once

#include "World/Actor/Mob/Hostile/HostileActor.h"

class PhantomActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:phantom";

    using HostileActor::HostileActor;

    ActorSize getSize() const override {
        return ActorSize{0.9f, 0.5f};
    }

    float getDefaultMaxHealth() const override {
        return 20.0f;
    }

    bool hasGravity() const override {
        return false;
    }

    // the game's insomnia: the spot only counts near a player under open sky who has not slept for days; the
    // phantom then goes up above that player
    bool placeNaturalSpawn(ServerNetworkHandler &owner, Level &level, Vector3f &position,
                           std::mt19937 &random) const override;
};
