#include "Actor/Mob/Hostile/PhantomActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(PhantomActor, PhantomActor::IDENTIFIER);

const std::vector<LootEntry> &PhantomActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:phantom_membrane", nullptr, 0, 1, 0.5f}
    };

    return entries;
}
