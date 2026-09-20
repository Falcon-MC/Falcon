#include "Actor/Mob/Hostile/SpiderActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(SpiderActor, SpiderActor::IDENTIFIER);

const std::vector<LootEntry> &SpiderActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:string", nullptr, 1, 2, 0.7f},
            {"minecraft:spider_eye", nullptr, 1, 1, 0.5f}
    };

    return entries;
}
