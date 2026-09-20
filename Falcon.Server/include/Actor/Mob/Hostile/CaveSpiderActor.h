#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class CaveSpiderActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:cave_spider";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.7f, 0.5f}; }

    float getDefaultMaxHealth() const override { return 12.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
