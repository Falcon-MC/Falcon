#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class ChickenActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:chicken";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 0.8f}; }

    float getDefaultMaxHealth() const override { return 4.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
