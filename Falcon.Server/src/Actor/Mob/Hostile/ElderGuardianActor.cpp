#include "Actor/Mob/Hostile/ElderGuardianActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ElderGuardianActor, ElderGuardianActor::IDENTIFIER);

const std::vector<LootEntry> &ElderGuardianActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:prismarine_shard", nullptr, 0, 2, 1.0f},
            {"minecraft:wet_sponge", nullptr, 1, 1, 1.0f},
            {"minecraft:cod", "minecraft:cooked_cod", 1, 1, 0.025f},
            {"minecraft:prismarine_crystals", nullptr, 0, 1, 0.3333f}
    };

    return entries;
}
