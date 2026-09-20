#include "Actor/Mob/Hostile/SlimeActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(SlimeActor, SlimeActor::IDENTIFIER);

const std::vector<LootEntry> &SlimeActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:slime_ball", nullptr, 1, 2, 1.0f}
    };

    return entries;
}
