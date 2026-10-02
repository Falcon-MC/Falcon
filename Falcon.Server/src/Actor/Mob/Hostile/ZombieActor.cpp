#include "Actor/Mob/Hostile/ZombieActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ZombieActor, ZombieActor::IDENTIFIER);

namespace {
    const float HAND_DAMAGE[] = {2.5f, 3.0f, 4.5f};
}

float ZombieActor::getAttackDamage(Difficulty difficulty) const {
    if (difficulty == Difficulty::Peaceful)
        return 0.0f;
    return HAND_DAMAGE[(int) difficulty - 1];
}
