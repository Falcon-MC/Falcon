#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class CodActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:cod";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 0.3f}; }

    float getDefaultMaxHealth() const override { return 3.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
