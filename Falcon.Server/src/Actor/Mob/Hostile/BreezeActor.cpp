#include "Actor/Mob/Hostile/BreezeActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(BreezeActor, BreezeActor::IDENTIFIER);

const std::vector<LootEntry> &BreezeActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:breeze_rod", nullptr, 1, 2, 1.0f}
    };

    return entries;
}
