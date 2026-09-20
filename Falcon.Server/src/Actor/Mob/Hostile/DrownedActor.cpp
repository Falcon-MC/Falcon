#include "Actor/Mob/Hostile/DrownedActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(DrownedActor, DrownedActor::IDENTIFIER);

const std::vector<LootEntry> &DrownedActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rotten_flesh", nullptr, 1, 1, 1.0f}
    };

    return entries;
}
