#include "Actor/Mob/Hostile/BlazeActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(BlazeActor, BlazeActor::IDENTIFIER);

const std::vector<LootEntry> &BlazeActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:blaze_rod", nullptr, 1, 1, 0.5f}
    };

    return entries;
}
