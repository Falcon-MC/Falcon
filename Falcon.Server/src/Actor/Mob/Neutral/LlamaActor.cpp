#include "Actor/Mob/Neutral/LlamaActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(LlamaActor, LlamaActor::IDENTIFIER);

const std::vector<LootEntry> &LlamaActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
    };

    return entries;
}
