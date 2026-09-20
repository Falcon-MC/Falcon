#include "Actor/Mob/Passive/PigActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(PigActor, PigActor::IDENTIFIER);

const std::vector<LootEntry> &PigActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:porkchop", "minecraft:cooked_porkchop", 1, 3, 1.0f}
    };

    return entries;
}
