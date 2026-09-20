#include "Actor/Mob/Hostile/CaveSpiderActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(CaveSpiderActor, CaveSpiderActor::IDENTIFIER);

const std::vector<LootEntry> &CaveSpiderActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:string", nullptr, 0, 2, 1.0f},
            {"minecraft:spider_eye", nullptr, 1, 1, 0.5f}
    };

    return entries;
}
