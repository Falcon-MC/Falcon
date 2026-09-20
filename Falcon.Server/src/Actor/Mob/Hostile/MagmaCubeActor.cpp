#include "Actor/Mob/Hostile/MagmaCubeActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(MagmaCubeActor, MagmaCubeActor::IDENTIFIER);

const std::vector<LootEntry> &MagmaCubeActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:magma_cream", nullptr, 0, 2, 0.6667f}
    };

    return entries;
}
