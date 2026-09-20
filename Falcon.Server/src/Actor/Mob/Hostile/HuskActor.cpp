#include "Actor/Mob/Hostile/HuskActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(HuskActor, HuskActor::IDENTIFIER);

const std::vector<LootEntry> &HuskActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rotten_flesh", nullptr, 0, 3, 1.0f}
    };

    return entries;
}
