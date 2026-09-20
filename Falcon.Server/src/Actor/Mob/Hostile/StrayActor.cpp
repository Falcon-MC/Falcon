#include "Actor/Mob/Hostile/StrayActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(StrayActor, StrayActor::IDENTIFIER);

const std::vector<LootEntry> &StrayActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:bone", nullptr, 1, 2, 0.66f},
            {"minecraft:arrow", nullptr, 1, 2, 0.55f}
    };

    return entries;
}
