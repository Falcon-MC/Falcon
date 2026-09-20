#include "Actor/Mob/Hostile/GhastActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(GhastActor, GhastActor::IDENTIFIER);

const std::vector<LootEntry> &GhastActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:ghast_tear", nullptr, 0, 1, 0.5f},
            {"minecraft:gunpowder", nullptr, 0, 2, 0.6667f}
    };

    return entries;
}
