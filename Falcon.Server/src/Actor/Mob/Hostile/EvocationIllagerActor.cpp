#include "Actor/Mob/Hostile/EvocationIllagerActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(EvocationIllagerActor, EvocationIllagerActor::IDENTIFIER);

const std::vector<LootEntry> &EvocationIllagerActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:totem_of_undying", nullptr, 1, 1, 1.0f},
            {"minecraft:emerald", nullptr, 0, 2, 1.0f}
    };

    return entries;
}
