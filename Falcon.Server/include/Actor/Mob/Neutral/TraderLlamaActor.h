#pragma once

#include "Actor/Mob/Neutral/NeutralActor.h"

class TraderLlamaActor : public NeutralActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:trader_llama";

    using NeutralActor::NeutralActor;

    ActorSize getSize() const override { return ActorSize{0.6f, 1.9f}; }

    float getDefaultMaxHealth() const override { return 20.0f; }
};
