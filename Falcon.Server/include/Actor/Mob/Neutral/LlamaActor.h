#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class LlamaActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:llama";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    static constexpr int MINIMUM_HEALTH = 15;

    static constexpr int MAXIMUM_HEALTH = 30;

    float getDefaultMaxHealth() const override { return (float) MINIMUM_HEALTH; }

    float resolveMaxHealth(Difficulty difficulty) const override {
        (void) difficulty;
        return (float) randomRange(MINIMUM_HEALTH, MAXIMUM_HEALTH);
    }

    const std::vector<LootEntry> &getLootEntries() const override;
};
