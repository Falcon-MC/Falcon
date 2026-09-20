#include "Actor/Mob/Hostile/RavagerActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(RavagerActor, RavagerActor::IDENTIFIER);

const std::vector<LootEntry> &RavagerActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:saddle", nullptr, 1, 1, 1.0f}
    };

    return entries;
}
