#include "Actor/Mob/Passive/ChickenActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ChickenActor, ChickenActor::IDENTIFIER);

const std::vector<LootEntry> &ChickenActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:chicken", "minecraft:cooked_chicken", 1, 1, 1.0f},
            {"minecraft:feather", nullptr, 0, 2, 1.0f}
    };

    return entries;
}
