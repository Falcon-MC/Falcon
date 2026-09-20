#include "Actor/Mob/Passive/SalmonActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(SalmonActor, SalmonActor::IDENTIFIER);

const std::vector<LootEntry> &SalmonActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:salmon", "minecraft:cooked_salmon", 1, 1, 1.0f},
            {"minecraft:bone", nullptr, 1, 2, 0.25f}
    };

    return entries;
}
