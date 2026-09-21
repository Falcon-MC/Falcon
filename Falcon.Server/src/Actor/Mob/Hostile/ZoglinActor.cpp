#include "Actor/Mob/Hostile/ZoglinActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ZoglinActor, ZoglinActor::IDENTIFIER);

int ZoglinActor::getExperienceDrop() const {
    return randomRange(1, 3);
}
