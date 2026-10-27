#pragma once

#include "World/Actor/Mob/Passive/AbstractHorseActor.h"

class HorseActor : public AbstractHorseActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:horse";

    using AbstractHorseActor::AbstractHorseActor;
};
