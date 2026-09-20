#include "Actor/Mob/Hostile/ZombieNautilusActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ZombieNautilusActor, ZombieNautilusActor::IDENTIFIER);

const std::vector<LootEntry> &ZombieNautilusActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rotten_flesh", nullptr, 0, 3, 1.0f}
    };

    return entries;
}
