#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class ZombieNautilusActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:zombie_nautilus";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.875f, 0.95f}; }

    float getDefaultMaxHealth() const override { return 15.0f; }

    int getExperienceDrop() const override { return 0; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
