#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class RavagerActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:ravager";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{1.2f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 100.0f; }

    int getExperienceDrop() const override { return 20; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
