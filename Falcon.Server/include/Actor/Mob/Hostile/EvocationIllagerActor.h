#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class EvocationIllagerActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:evocation_illager";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 24.0f; }

    int getExperienceDrop() const override { return 10; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
