#include "Actor/Mob/Passive/SkeletonHorseActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(SkeletonHorseActor, SkeletonHorseActor::IDENTIFIER);

const std::vector<LootEntry> &SkeletonHorseActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:bone", nullptr, 0, 2, 0.6667f}
    };

    return entries;
}
