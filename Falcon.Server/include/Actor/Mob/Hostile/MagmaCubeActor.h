#pragma once

#include "Actor/Mob/Hostile/AbstractSlimeActor.h"

class MagmaCubeActor : public AbstractSlimeActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:magma_cube";

    using AbstractSlimeActor::AbstractSlimeActor;
};
