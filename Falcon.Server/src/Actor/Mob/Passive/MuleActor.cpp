#include "Actor/Mob/Passive/MuleActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(MuleActor, MuleActor::IDENTIFIER);

const std::vector<LootEntry> &MuleActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
    };

    return entries;
}
