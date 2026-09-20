#include "Actor/Mob/Neutral/EndermanActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(EndermanActor, EndermanActor::IDENTIFIER);

const std::vector<LootEntry> &EndermanActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:ender_pearl", nullptr, 1, 1, 0.5f}
    };

    return entries;
}
