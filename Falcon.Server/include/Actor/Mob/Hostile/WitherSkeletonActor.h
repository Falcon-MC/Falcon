#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class WitherSkeletonActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:wither_skeleton";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.7f, 2.4f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
