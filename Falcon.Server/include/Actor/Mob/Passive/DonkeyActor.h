#pragma once

#include "Actor/Mob/Passive/AbstractHorseActor.h"

class DonkeyActor : public AbstractHorseActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:donkey";

    using AbstractHorseActor::AbstractHorseActor;

    const std::vector<LootEntry> &getLootEntries() const override;
};
