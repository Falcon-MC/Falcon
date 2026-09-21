#include "Actor/Mob/Hostile/SlimeActor.h"

#include "Actor/ActorClassRegistry.h"

float SlimeActor::getContactDamage() const {
    if (getSizeVariant() == LARGE_SIZE)
        return 4.0f;
    if (getSizeVariant() == MEDIUM_SIZE)
        return 2.0f;
    return 0.0f;
}

const LootTable *SlimeActor::getLootTable() const {
    return getSizeVariant() == SMALL_SIZE ? AbstractSlimeActor::getLootTable() : nullptr;
}

FALCON_REGISTER_ACTOR(SlimeActor, SlimeActor::IDENTIFIER);
