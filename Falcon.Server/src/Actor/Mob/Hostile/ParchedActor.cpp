#include "Actor/Mob/Hostile/ParchedActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ParchedActor, ParchedActor::IDENTIFIER);

const std::vector<LootEntry> &ParchedActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:bone", nullptr, 0, 2, 1.0f},
            {"minecraft:arrow", nullptr, 0, 2, 1.0f},
            {"minecraft:bow", nullptr, 1, 1, 0.08f, 0.05f}
    };

    return entries;
}
