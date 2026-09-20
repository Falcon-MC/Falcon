#include "Actor/Mob/Hostile/ShulkerActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ShulkerActor, ShulkerActor::IDENTIFIER);

const std::vector<LootEntry> &ShulkerActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:shulker_shell", nullptr, 0, 1, 0.5f}
    };

    return entries;
}
