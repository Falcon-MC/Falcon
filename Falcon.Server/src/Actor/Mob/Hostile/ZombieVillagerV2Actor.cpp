#include "Actor/Mob/Hostile/ZombieVillagerV2Actor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ZombieVillagerV2Actor, ZombieVillagerV2Actor::IDENTIFIER);

const std::vector<LootEntry> &ZombieVillagerV2Actor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rotten_flesh", nullptr, 0, 3, 1.0f}
    };

    return entries;
}
