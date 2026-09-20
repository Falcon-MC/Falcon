#include "Actor/Mob/Passive/HorseActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(HorseActor, HorseActor::IDENTIFIER);

const std::vector<LootEntry> &HorseActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
    };

    return entries;
}
