#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class CreeperActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:creeper";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.8f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
