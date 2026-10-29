#pragma once

struct PlayerMovementContext;

class PlayerGravitySystem {
public:
    static void tick(PlayerMovementContext &context);
};
