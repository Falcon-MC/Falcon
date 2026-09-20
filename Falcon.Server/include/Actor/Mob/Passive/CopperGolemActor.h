#pragma once

#include "Actor/Mob/Passive/PassiveActor.h"

class CopperGolemActor : public PassiveActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:copper_golem";

    using PassiveActor::PassiveActor;

    ActorSize getSize() const override { return ActorSize{0.49f, 0.98f}; }

    float getDefaultMaxHealth() const override { return 12.0f; }

    int getExperienceDrop() const override { return 0; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
