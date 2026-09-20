#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class PolarBearActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:polar_bear";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{1.3f, 1.4f}; }

    float getDefaultMaxHealth() const override { return 30.0f; }

    const std::vector<LootEntry> &getLootEntries() const override;
};
