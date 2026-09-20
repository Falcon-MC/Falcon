#include "Actor/Mob/Passive/SnowGolemActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(SnowGolemActor, SnowGolemActor::IDENTIFIER);

const std::vector<LootEntry> &SnowGolemActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:snowball", nullptr, 0, 15, 0.6667f}
    };

    return entries;
}
