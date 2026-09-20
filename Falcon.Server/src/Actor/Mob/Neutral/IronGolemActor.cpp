#include "Actor/Mob/Neutral/IronGolemActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(IronGolemActor, IronGolemActor::IDENTIFIER);

const std::vector<LootEntry> &IronGolemActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:iron_ingot", nullptr, 3, 5, 1.0f},
            {"minecraft:poppy", nullptr, 0, 2, 1.0f}
    };

    return entries;
}
