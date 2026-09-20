#include "Actor/Mob/Passive/RabbitActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(RabbitActor, RabbitActor::IDENTIFIER);

const std::vector<LootEntry> &RabbitActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rabbit_hide", nullptr, 0, 1, 0.5f},
            {"minecraft:rabbit", "minecraft:cooked_rabbit", 0, 1, 0.5f},
            {"minecraft:rabbit_foot", nullptr, 1, 1, 0.1f, 0.03f}
    };

    return entries;
}
