#pragma once

#include "World/Actor/Mob/Passive/AbstractHorseActor.h"

class MuleActor : public AbstractHorseActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:mule";

    using AbstractHorseActor::AbstractHorseActor;
};
