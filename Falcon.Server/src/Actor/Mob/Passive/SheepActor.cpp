#include "Actor/Mob/Passive/SheepActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(SheepActor, SheepActor::IDENTIFIER);

const std::vector<LootEntry> &SheepActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:mutton", "minecraft:cooked_mutton", 1, 2, 1.0f},
            {"minecraft:white_wool", nullptr, 1, 1, 1.0f}
    };

    return entries;
}
