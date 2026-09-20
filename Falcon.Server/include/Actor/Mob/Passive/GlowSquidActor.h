#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class GlowSquidActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:glow_squid";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.475f, 0.95f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
