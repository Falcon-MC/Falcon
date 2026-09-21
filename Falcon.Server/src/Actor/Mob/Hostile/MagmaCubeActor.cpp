#include "Actor/Mob/Hostile/MagmaCubeActor.h"

#include "Actor/ActorClassRegistry.h"

float MagmaCubeActor::getContactDamage() const {
    if (getSizeVariant() == LARGE_SIZE)
        return 6.0f;
    if (getSizeVariant() == MEDIUM_SIZE)
        return 4.0f;
    return 3.0f;
}

const LootTable *MagmaCubeActor::getLootTable() const {
    return getSizeVariant() > SMALL_SIZE ? AbstractSlimeActor::getLootTable() : nullptr;
}

FALCON_REGISTER_ACTOR(MagmaCubeActor, MagmaCubeActor::IDENTIFIER);
