#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class BreezeActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:breeze";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.77f}; }

    float getDefaultMaxHealth() const override { return 30.0f; }

    int getExperienceDrop() const override { return 10; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
