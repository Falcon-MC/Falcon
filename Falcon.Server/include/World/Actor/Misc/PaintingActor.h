#pragma once

#include "World/Actor/ServerActor.h"

class PaintingActor final : public ServerActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:painting";

    using ServerActor::ServerActor;
};
