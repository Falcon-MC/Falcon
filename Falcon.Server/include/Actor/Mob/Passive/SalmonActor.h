#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class SalmonActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:salmon";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.5f, 0.5f}; }

    float getDefaultMaxHealth() const override { return 3.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
