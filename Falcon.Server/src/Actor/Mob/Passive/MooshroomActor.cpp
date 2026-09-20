#include "Actor/Mob/Passive/MooshroomActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(MooshroomActor, MooshroomActor::IDENTIFIER);

const std::vector<LootEntry> &MooshroomActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:beef", "minecraft:cooked_beef", 1, 3, 1.0f},
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
    };

    return entries;
}
