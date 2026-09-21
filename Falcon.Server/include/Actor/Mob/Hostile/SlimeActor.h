#pragma once

#include "Actor/Mob/Hostile/AbstractSlimeActor.h"

class SlimeActor : public AbstractSlimeActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:slime";

    using AbstractSlimeActor::AbstractSlimeActor;

    float getContactDamage() const override;

    const LootTable *getLootTable() const override;
};
