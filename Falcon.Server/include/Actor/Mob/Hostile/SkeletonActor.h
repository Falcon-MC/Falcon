#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class SkeletonActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:skeleton";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
