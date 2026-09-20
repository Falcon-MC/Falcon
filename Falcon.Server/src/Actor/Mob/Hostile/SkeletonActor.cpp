#include "Actor/Mob/Hostile/SkeletonActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(SkeletonActor, SkeletonActor::IDENTIFIER);

const std::vector<LootEntry> &SkeletonActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:bone", nullptr, 0, 2, 1.0f},
            {"minecraft:arrow", nullptr, 0, 2, 1.0f}
    };

    return entries;
}
