#include "Actor/Mob/Neutral/PandaActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(PandaActor, PandaActor::IDENTIFIER);

const std::vector<LootEntry> &PandaActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:bamboo", nullptr, 0, 3, 1.0f}
    };

    return entries;
}
