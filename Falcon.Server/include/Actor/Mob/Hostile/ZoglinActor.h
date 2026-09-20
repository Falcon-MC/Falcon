#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class ZoglinActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:zoglin";

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{1.4f, 1.4f}; }

    float getDefaultMaxHealth() const override { return 40.0f; }

    int getExperienceDrop() const override;

    const std::vector<LootEntry> &getLootEntries() const override;
};
