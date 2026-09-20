#include "Actor/Mob/Hostile/HoglinActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(HoglinActor, HoglinActor::IDENTIFIER);

int HoglinActor::getExperienceDrop() const {
    return randomRange(1, 3);
}

const std::vector<LootEntry> &HoglinActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:porkchop", "minecraft:cooked_porkchop", 2, 4, 1.0f},
            {"minecraft:leather", nullptr, 0, 1, 0.5f}
    };

    return entries;
}
