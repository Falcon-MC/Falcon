#include "Actor/Mob/Hostile/PillagerActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(PillagerActor, PillagerActor::IDENTIFIER);

const std::vector<LootEntry> &PillagerActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:arrow", nullptr, 0, 2, 1.0f}
    };

    return entries;
}
