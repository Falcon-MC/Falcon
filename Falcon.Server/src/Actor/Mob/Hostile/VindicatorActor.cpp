#include "Actor/Mob/Hostile/VindicatorActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(VindicatorActor, VindicatorActor::IDENTIFIER);

const std::vector<LootEntry> &VindicatorActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:iron_axe", nullptr, 1, 1, 1.0f},
            {"minecraft:emerald", nullptr, 0, 2, 1.0f}
    };

    return entries;
}
