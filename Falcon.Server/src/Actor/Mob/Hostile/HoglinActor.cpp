#include "Actor/Mob/Hostile/HoglinActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(HoglinActor, HoglinActor::IDENTIFIER);

int HoglinActor::getExperienceDrop() const {
    return randomRange(1, 3);
}
