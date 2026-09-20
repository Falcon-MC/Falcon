#include "Actor/Mob/Passive/CatActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(CatActor, CatActor::IDENTIFIER);

const std::vector<LootEntry> &CatActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:string", nullptr, 0, 2, 1.0f}
    };

    return entries;
}
