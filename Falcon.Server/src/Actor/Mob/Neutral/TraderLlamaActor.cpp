#include "Actor/Mob/Neutral/TraderLlamaActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(TraderLlamaActor, TraderLlamaActor::IDENTIFIER);

const std::vector<LootEntry> &TraderLlamaActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
    };

    return entries;
}
