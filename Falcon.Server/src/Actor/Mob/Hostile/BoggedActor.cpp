#include "Actor/Mob/Hostile/BoggedActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(BoggedActor, BoggedActor::IDENTIFIER);

const std::vector<LootEntry> &BoggedActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:bone", nullptr, 0, 2, 1.0f},
            {"minecraft:arrow", nullptr, 0, 2, 1.0f}
    };

    return entries;
}
