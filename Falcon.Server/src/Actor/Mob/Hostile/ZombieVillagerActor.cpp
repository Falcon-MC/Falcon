#include "Actor/Mob/Hostile/ZombieVillagerActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ZombieVillagerActor, ZombieVillagerActor::IDENTIFIER);

const std::vector<LootEntry> &ZombieVillagerActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rotten_flesh", nullptr, 0, 3, 1.0f}
    };

    return entries;
}
