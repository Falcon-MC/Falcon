#include "Actor/Mob/Hostile/CreeperActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(CreeperActor, CreeperActor::IDENTIFIER);

const std::vector<LootEntry> &CreeperActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:gunpowder", nullptr, 0, 2, 1.0f}
    };

    return entries;
}
