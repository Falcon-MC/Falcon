#include "Actor/Mob/Passive/CodActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(CodActor, CodActor::IDENTIFIER);

const std::vector<LootEntry> &CodActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:cod", "minecraft:cooked_cod", 1, 1, 1.0f},
            {"minecraft:bone", nullptr, 1, 2, 0.25f}
    };

    return entries;
}
