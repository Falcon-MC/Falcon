#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class GuardianActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:guardian";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.85f, 0.85f}; }

    float getDefaultMaxHealth() const override { return 30.0f; }

    int getExperienceDrop() const override { return 10; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
