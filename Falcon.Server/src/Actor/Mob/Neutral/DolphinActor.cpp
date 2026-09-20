#include "Actor/Mob/Neutral/DolphinActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(DolphinActor, DolphinActor::IDENTIFIER);

const std::vector<LootEntry> &DolphinActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:cod", "minecraft:cooked_cod", 0, 1, 0.5f}
    };

    return entries;
}
