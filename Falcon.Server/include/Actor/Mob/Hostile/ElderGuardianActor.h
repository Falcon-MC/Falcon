#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class ElderGuardianActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:elder_guardian";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{1.99f, 1.99f}; }

    float getDefaultMaxHealth() const override { return 80.0f; }

    int getExperienceDrop() const override { return 10; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
