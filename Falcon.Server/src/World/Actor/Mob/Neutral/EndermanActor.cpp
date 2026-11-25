#include "World/Actor/Mob/Neutral/EndermanActor.h"

#include "World/Actor/ActorClassRegistry.h"

#include <random>

FALCON_REGISTER_ACTOR(EndermanActor, EndermanActor::IDENTIFIER);

namespace {
    // one hurt in this many it lives through leaves it in place
    const int32_t STAY_CHANCE = 10;

    std::mt19937 &hurtRandom() {
        static std::mt19937 generator{std::random_device{}()};
        return generator;
    }
}

// the game tries a random teleport on a hurt it survives: this takes it out of water too
void EndermanActor::onDamaged(ServerNetworkHandler &owner, Actor *attacker) {
    NeutralActor::onDamaged(owner, attacker);
    if (getHealth() > 0.0f && std::uniform_int_distribution<int32_t>(0, STAY_CHANCE - 1)(hurtRandom()) != 0)
        randomTeleport(owner);
}
