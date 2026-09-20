#include "Actor/Mob/Neutral/PolarBearActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(PolarBearActor, PolarBearActor::IDENTIFIER);

const std::vector<LootEntry> &PolarBearActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:cod", "minecraft:cooked_cod", 0, 2, 0.6667f},
            {"minecraft:salmon", "minecraft:cooked_salmon", 0, 2, 0.6667f}
    };

    return entries;
}
