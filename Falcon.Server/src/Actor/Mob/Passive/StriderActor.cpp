#include "Actor/Mob/Passive/StriderActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(StriderActor, StriderActor::IDENTIFIER);

const std::vector<LootEntry> &StriderActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:string", nullptr, 2, 5, 1.0f}
    };

    return entries;
}
