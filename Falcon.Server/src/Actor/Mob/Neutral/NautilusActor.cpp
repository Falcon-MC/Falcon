#include "Actor/Mob/Neutral/NautilusActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(NautilusActor, NautilusActor::IDENTIFIER);

const std::vector<LootEntry> &NautilusActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:nautilus_shell", nullptr, 1, 1, 0.05f}
    };

    return entries;
}
