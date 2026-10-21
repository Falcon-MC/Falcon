#pragma once

struct PlayerMovementContext;

class FlyingPlayerStuckOnGroundWorkaroundSystem {
public:
    static void tick(PlayerMovementContext &context);
};
