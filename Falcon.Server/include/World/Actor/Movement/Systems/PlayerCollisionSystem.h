#pragma once

struct PlayerMovementContext;

class PlayerCollisionSystem {
public:
    static void tick(PlayerMovementContext &context);
};
