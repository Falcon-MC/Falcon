#include "Actor/Mob/Passive/ParrotActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ParrotActor, ParrotActor::IDENTIFIER);

const std::vector<LootEntry> &ParrotActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:feather", nullptr, 1, 2, 1.0f}
    };

    return entries;
}
