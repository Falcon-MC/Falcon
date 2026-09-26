#pragma once

#include "Actor/Mob/Hostile/AbstractSlimeActor.h"

class MagmaCubeActor : public AbstractSlimeActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:magma_cube";

    using AbstractSlimeActor::AbstractSlimeActor;

    float getContactDamage() const override;

    float getHopPower() const override {
        return BASE_HOP_POWER + (float) getSizeVariant() * SIZE_HOP_BONUS;
    }

    static constexpr float SIZE_HOP_BONUS = 0.1f;

    const LootTable *getLootTable() const override;
};
