#pragma once

struct PlayerMovementContext;

class PlayerLiquidContactSystem {
public:
    static void tick(PlayerMovementContext &context);
};
