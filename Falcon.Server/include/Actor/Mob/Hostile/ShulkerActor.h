#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class ShulkerActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:shulker";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{0.99f, 0.99f}; }

    float getDefaultMaxHealth() const override { return 30.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
