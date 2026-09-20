#include "Actor/Mob/Passive/DonkeyActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(DonkeyActor, DonkeyActor::IDENTIFIER);

const std::vector<LootEntry> &DonkeyActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
    };

    return entries;
}
