#include "Actor/Mob/Passive/SquidActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(SquidActor, SquidActor::IDENTIFIER);

const std::vector<LootEntry> &SquidActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:ink_sac", nullptr, 1, 3, 1.0f}
    };

    return entries;
}
