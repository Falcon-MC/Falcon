#include "Actor/Mob/Hostile/GuardianActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(GuardianActor, GuardianActor::IDENTIFIER);

const std::vector<LootEntry> &GuardianActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:prismarine_shard", nullptr, 0, 2, 1.0f},
            {"minecraft:cod", "minecraft:cooked_cod", 1, 1, 0.025f},
            {"minecraft:prismarine_crystals", nullptr, 0, 1, 0.3333f}
    };

    return entries;
}
