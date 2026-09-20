#include "Actor/Mob/Passive/TurtleActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(TurtleActor, TurtleActor::IDENTIFIER);

const std::vector<LootEntry> &TurtleActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:seagrass", nullptr, 0, 2, 1.0f}
    };

    return entries;
}
