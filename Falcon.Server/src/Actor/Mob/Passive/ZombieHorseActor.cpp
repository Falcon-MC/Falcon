#include "Actor/Mob/Passive/ZombieHorseActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ZombieHorseActor, ZombieHorseActor::IDENTIFIER);

const std::vector<LootEntry> &ZombieHorseActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rotten_flesh", nullptr, 2, 3, 1.0f}
    };

    return entries;
}
