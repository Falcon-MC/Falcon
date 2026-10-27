#pragma once

struct PlayerMovementContext;

class MovementInputSystem {
public:
    static void tick(PlayerMovementContext &context);
};
