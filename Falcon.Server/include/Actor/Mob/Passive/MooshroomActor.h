#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class MooshroomActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:mooshroom";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.9f, 1.3f}; }

    float getDefaultMaxHealth() const override { return 10.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
