#include "Actor/Mob/Hostile/ZombieActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ZombieActor, ZombieActor::IDENTIFIER);

const std::vector<LootEntry> &ZombieActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rotten_flesh", nullptr, 0, 3, 1.0f}
    };

    return entries;
}
