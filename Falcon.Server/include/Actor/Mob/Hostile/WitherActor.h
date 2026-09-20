#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class WitherActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:wither";

    static constexpr float EASY_HEALTH = 300.0f;

    static constexpr float NORMAL_HEALTH = 450.0f;

    static constexpr float HARD_HEALTH = 600.0f;

    using HostileActor::HostileActor;

    ActorSize getSize() const override { return ActorSize{1.0f, 3.0f}; }

    float getDefaultMaxHealth() const override { return EASY_HEALTH; }

    float resolveMaxHealth(Difficulty difficulty) const override;

    int getExperienceDrop() const override { return 50; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
