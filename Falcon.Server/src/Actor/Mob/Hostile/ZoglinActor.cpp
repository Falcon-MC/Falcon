#include "Actor/Mob/Hostile/ZoglinActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ZoglinActor, ZoglinActor::IDENTIFIER);

int ZoglinActor::getExperienceDrop() const {
    return randomRange(1, 3);
}

const std::vector<LootEntry> &ZoglinActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:rotten_flesh", nullptr, 1, 3, 1.0f}
    };

    return entries;
}
