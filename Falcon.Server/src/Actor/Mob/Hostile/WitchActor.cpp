#include "Actor/Mob/Hostile/WitchActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(WitchActor, WitchActor::IDENTIFIER);

const std::vector<LootEntry> &WitchActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:redstone", nullptr, 4, 8, 1.0f},
            {"minecraft:stick", nullptr, 0, 6, 0.3349f},
            {"minecraft:spider_eye", nullptr, 0, 6, 0.1787f},
            {"minecraft:glowstone_dust", nullptr, 0, 6, 0.1787f},
            {"minecraft:gunpowder", nullptr, 0, 6, 0.1787f},
            {"minecraft:sugar", nullptr, 0, 6, 0.1787f},
            {"minecraft:glass_bottle", nullptr, 0, 6, 0.1787f}
    };

    return entries;
}
