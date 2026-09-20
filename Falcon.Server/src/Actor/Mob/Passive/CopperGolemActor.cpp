#include "Actor/Mob/Passive/CopperGolemActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(CopperGolemActor, CopperGolemActor::IDENTIFIER);

const std::vector<LootEntry> &CopperGolemActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:copper_ingot", nullptr, 1, 3, 1.0f}
    };

    return entries;
}
