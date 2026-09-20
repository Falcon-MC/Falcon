#include "Actor/Mob/Neutral/ZombiePigmanActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ZombiePigmanActor, ZombiePigmanActor::IDENTIFIER);

const std::vector<LootEntry> &ZombiePigmanActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rotten_flesh", nullptr, 0, 1, 1.0f},
            {"minecraft:gold_nugget", nullptr, 0, 1, 1.0f}
    };

    return entries;
}
