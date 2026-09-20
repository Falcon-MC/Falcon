#include "Actor/Mob/Passive/CamelHuskActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(CamelHuskActor, CamelHuskActor::IDENTIFIER);

const std::vector<LootEntry> &CamelHuskActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rotten_flesh", nullptr, 2, 3, 0.6667f}
    };

    return entries;
}
