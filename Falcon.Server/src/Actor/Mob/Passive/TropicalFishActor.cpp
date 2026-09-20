#include "Actor/Mob/Passive/TropicalFishActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(TropicalFishActor, TropicalFishActor::IDENTIFIER);

const std::vector<LootEntry> &TropicalFishActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:tropical_fish", nullptr, 1, 1, 1.0f},
            {"minecraft:bone", nullptr, 1, 2, 0.25f}
    };

    return entries;
}
