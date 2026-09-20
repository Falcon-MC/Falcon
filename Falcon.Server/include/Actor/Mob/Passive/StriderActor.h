#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class StriderActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:strider";

    int getExperienceDrop() const override { return randomRange(1, 2); }

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.9f, 1.7f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
