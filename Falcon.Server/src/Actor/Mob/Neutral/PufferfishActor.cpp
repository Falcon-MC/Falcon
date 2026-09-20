#include "Actor/Mob/Neutral/PufferfishActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(PufferfishActor, PufferfishActor::IDENTIFIER);

const std::vector<LootEntry> &PufferfishActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:pufferfish", nullptr, 1, 1, 1.0f},
            {"minecraft:bone", nullptr, 1, 2, 0.25f}
    };

    return entries;
}
