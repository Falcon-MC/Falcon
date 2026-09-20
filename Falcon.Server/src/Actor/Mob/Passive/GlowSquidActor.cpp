#include "Actor/Mob/Passive/GlowSquidActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(GlowSquidActor, GlowSquidActor::IDENTIFIER);

const std::vector<LootEntry> &GlowSquidActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:glow_ink_sac", nullptr, 1, 3, 1.0f}
    };

    return entries;
}
