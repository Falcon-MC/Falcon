#pragma once

struct PlayerMovementContext;

class LavaDragSystem {
public:
    static void tick(PlayerMovementContext &context);
};
